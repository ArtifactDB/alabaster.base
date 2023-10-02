#ifndef UZUKI2_PARSE_HPP
#define UZUKI2_PARSE_HPP

#include <memory>
#include <vector>
#include <cctype>
#include <string>
#include <cstring>
#include <stdexcept>
#include <cstdint>
#include <unordered_set>

#include "H5Cpp.h"

#include "interfaces.hpp"
#include "Dummy.hpp"
#include "utils.hpp"
#include "Version.hpp"

/**
 * @file parse_hdf5.hpp
 * @brief Parsing methods for HDF5 files.
 */

namespace uzuki2 {

/**
 * @namespace uzuki2::hdf5
 * @brief Parse an R list from a HDF5 file.
 *
 * The hierarchical nature of HDF5 allows it to naturally store nested list structures.
 * It supports random access of list components, which provides some optimization opportunities for parsing large lists.
 * However, it incurs a large overhead per list element; for small lists, users may prefer to use a JSON file instead (see `json`).
 */
namespace hdf5 {

/**
 * @cond
 */
inline std::string load_string_attribute(const H5::Attribute& attr, const std::string& field, const std::string& path) {
    if (attr.getTypeClass() != H5T_STRING || attr.getSpace().getSimpleExtentNdims() != 0) {
        throw std::runtime_error(std::string("'") + field + "' attribute should be a scalar string at '" + path + "'");
    }

    std::string output;
    attr.read(attr.getStrType(), output);
    return output;
}

template<class H5Object>
std::string load_string_attribute(const H5Object& handle, const std::string& field, const std::string& path) {
    if (!handle.attrExists(field)) {
        throw std::runtime_error(std::string("expected a '") + field + "' attribute at '" + path + "'");
    }
    return load_string_attribute(handle.openAttribute(field), field, path);
}

template<class Function>
void load_string_dataset(const H5::DataSet& handle, hsize_t full_length, Function fun) {
    auto dtype = handle.getDataType();

    // TODO: read this in chunks.
    if (dtype.isVariableStr()) {
        std::vector<char*> buffer(full_length);
        handle.read(buffer.data(), dtype);

        for (size_t i = 0; i < full_length; ++i) {
            fun(i, std::string(buffer[i]));
        }

        auto dspace = handle.getSpace();
        H5Dvlen_reclaim(dtype.getId(), dspace.getId(), H5P_DEFAULT, buffer.data());

    } else {
        size_t len = dtype.getSize();
        std::vector<char> buffer(len * full_length);
        handle.read(buffer.data(), dtype);

        auto start = buffer.data();
        for (size_t i = 0; i < full_length; ++i, start += len) {
            size_t j = 0;
            for (; j < len && start[j] != '\0'; ++j) {}
            fun(i, std::string(start, start + j));
        }
    }
}

inline H5::DataSet get_scalar_dataset(const H5::Group& handle, const std::string& name, H5T_class_t type_class, const std::string& path) {
    if (handle.childObjType(name) != H5O_TYPE_DATASET) {
        throw std::runtime_error("expected a dataset at '" + path + "/" + name + "'");
    }

    auto dhandle = handle.openDataSet(name);
    auto dtype = dhandle.getDataType();
    if (dtype.getClass() != type_class) {
        throw std::runtime_error("dataset at '" + path + "/" + name + "' has the wrong datatype class");
    }

    auto dspace = dhandle.getSpace();
    int ndims = dspace.getSimpleExtentNdims();
    if (ndims != 0) {
        throw std::runtime_error("expected a scalar dataset at '" + path + "/" + name + "'");
    }

    return dhandle;
}

inline hsize_t check_1d_length(const H5::DataSet& handle, const std::string& path, bool allow_scalar) {
    auto dspace = handle.getSpace();
    int ndims = dspace.getSimpleExtentNdims();

    if (ndims == 0 && allow_scalar) {
        return 0;
    }

    if (ndims != 1) {
        throw std::runtime_error("expected a 1-dimensional dataset at '" + path + "'");
    }

    hsize_t dims;
    dspace.getSimpleExtentDims(&dims);
    return dims;
}

inline void forbid_large_integers(const H5::DataSet& handle, const std::string& path) {
    H5::IntType itype(handle);

    bool failed = false;
    if (itype.getSign() == H5T_SGN_NONE) {
        if (itype.getPrecision() >= 32) {
            failed = true;
        }
    } else if (itype.getPrecision() > 32) {
        failed = true;
    }

    if (failed) {
        throw std::runtime_error("data type is potentially out of range of a 32-bit signed integer for '" + path + "'");
    }
}

template<class Host, class Function>
void parse_integer_like(const H5::DataSet& handle, Host* ptr, const std::string& path, Function check, const Version& version) {
    if (handle.getDataType().getClass() != H5T_INTEGER) {
        throw std::runtime_error("expected an integer dataset at '" + path + "'");
    }
    forbid_large_integers(handle, path);

    bool has_missing = false;
    int32_t missing_value = -2147483648;
    if (version.equals(1, 0)) {
        has_missing = true;
    } else {
        has_missing = handle.attrExists("missing-value-placeholder");
        if (has_missing) {
            auto attr = handle.openAttribute("missing-value-placeholder");
            if (attr.getTypeClass() != H5T_INTEGER || attr.getSpace().getSimpleExtentNdims() != 0) {
                throw std::runtime_error("'missing-value-placeholder' attribute should be a scalar integer at '" + path + "'");
            }
            attr.read(H5::PredType::NATIVE_INT32, &missing_value);
        }
    }

    // TODO: loop in chunks to reduce memory usage.
    size_t len = ptr->size();
    std::vector<int32_t> buffer(len);
    handle.read(buffer.data(), H5::PredType::NATIVE_INT32);
    for (hsize_t i = 0; i < len; ++i) {
        auto current = buffer[i];
        if (has_missing && current == missing_value) {
            ptr->set_missing(i);
        } else {
            check(current);
            ptr->set(i, current);
        }
    }
}

template<class Host, class Function>
void parse_string_like(const H5::DataSet& handle, Host* ptr, const std::string& path, Function check) {
    auto dtype = handle.getDataType();
    if (dtype.getClass() != H5T_STRING) {
        throw std::runtime_error("expected a string dataset at '" + path + "'");
    }

    const std::string placeholder = "missing-value-placeholder";
    bool has_missing = handle.attrExists(placeholder);
    std::string missing_val;
    if (has_missing) {
        auto ahandle = handle.openAttribute(placeholder);
        missing_val = load_string_attribute(ahandle, placeholder, path);
    }

    load_string_dataset(handle, ptr->size(), [&](size_t i, std::string x) -> void {
        if (has_missing && x == missing_val) {
            ptr->set_missing(i);
        } else {
            check(x);
            ptr->set(i, std::move(x));
        }
    });
}

inline double legacy_missing_double() {
    uint32_t tmp_value = 1;
    auto tmp_ptr = reinterpret_cast<unsigned char*>(&tmp_value);

    // Mimic R's generation of these values, but we can't use type punning as
    // this is not legal in C++, and we don't have bit_cast yet.
    double missing_value = 0;
    auto missing_ptr = reinterpret_cast<unsigned char*>(&missing_value);

    int step = 1;
    if (tmp_ptr[0] == 1) { // little-endian. 
        missing_ptr += sizeof(double) - 1;
        step = -1;
    }

    *missing_ptr = 0x7f;
    *(missing_ptr += step) = 0xf0;
    *(missing_ptr += step) = 0x00;
    *(missing_ptr += step) = 0x00;
    *(missing_ptr += step) = 0x00;
    *(missing_ptr += step) = 0x00;
    *(missing_ptr += step) = 0x07;
    *(missing_ptr += step) = 0xa2;

    return missing_value;
}

template<class Host, class Function>
void parse_numbers(const H5::DataSet& handle, Host* ptr, const std::string& path, Function check, const Version& version) {
    if (handle.getDataType().getClass() != H5T_FLOAT) {
        throw std::runtime_error("expected a float dataset at '" + path + "'");
    }

    H5::FloatType ftype(handle);
    if (ftype.getPrecision() > 64) {
        throw std::runtime_error("data type is potentially out of range for a double at '" + path + "'");
    }

    bool has_missing = false;
    double missing_value = 0;

    if (version.equals(1, 0)) {
        has_missing = true;
        missing_value = legacy_missing_double();
    } else {
        has_missing = handle.attrExists("missing-value-placeholder");
        if (has_missing) {
            auto attr = handle.openAttribute("missing-value-placeholder");
            if (attr.getTypeClass() != H5T_FLOAT || attr.getSpace().getSimpleExtentNdims() != 0) {
                throw std::runtime_error("'missing-value-placeholder' attribute should be a scalar float at '" + path + "'");
            }
            attr.read(H5::PredType::NATIVE_DOUBLE, &missing_value);
        }
    }

    auto missing_ptr = reinterpret_cast<unsigned char*>(&missing_value);
    auto is_missing = [&](double val) {
        // Can't compare directly as missing_value or val might be NaN,
        // so instead we need to do the comparison byte-by-byte.
        auto candidate_ptr = reinterpret_cast<unsigned char*>(&val);
        return std::memcmp(candidate_ptr, missing_ptr, sizeof(double)) == 0;
    };

    // TODO: loop in chunks to reduce memory usage.
    size_t len = ptr->size();
    std::vector<double> buffer(len);
    handle.read(buffer.data(), H5::PredType::NATIVE_DOUBLE);
    for (hsize_t i = 0; i < len; ++i) {
        auto current = buffer[i];
        if (has_missing && is_missing(current)) {
            ptr->set_missing(i);
        } else {
            check(current);
            ptr->set(i, current);
        }
    }
}

template<class Host>
void extract_names(const H5::Group& handle, Host* ptr, const std::string& path, const std::string& dpath) {
    auto npath = path + "/names";
    if (handle.childObjType("names") != H5O_TYPE_DATASET) {
        throw std::runtime_error("expected a dataset at '" + npath + "'");
    }

    auto nhandle = handle.openDataSet("names");
    auto dtype = nhandle.getDataType();
    if (dtype.getClass() != H5T_STRING) {
        throw std::runtime_error("expected a string dataset at '" + npath + "'");
    }

    auto len = ptr->size();
    auto nlen = check_1d_length(nhandle, npath, false);
    if (nlen != len) {
        throw std::runtime_error("length of '" + npath + "' should be equal to length of '" + dpath + "'");
    }

    load_string_dataset(nhandle, nlen, [&](size_t i, std::string x) -> void { ptr->set_name(i, x); });
}

template<class Provisioner, class Externals>
std::shared_ptr<Base> parse_inner(const H5::Group& handle, Externals& ext, const std::string& path, const Version& version) {
    // Deciding what type we're dealing with.
    auto object_type = load_string_attribute(handle, "uzuki_object", path);
    std::shared_ptr<Base> output;

    if (object_type == "list") {
        auto dpath = path + "/data";
        if (!handle.exists("data") || handle.childObjType("data") != H5O_TYPE_GROUP) {
            throw std::runtime_error("expected a group at '" + dpath + "'");
        }
        auto dhandle = handle.openGroup("data");
        size_t len = dhandle.getNumObjs();

        bool named = handle.exists("names");
        auto lptr = Provisioner::new_List(len, named);
        output.reset(lptr);

        for (size_t i = 0; i < len; ++i) {
            auto istr = std::to_string(i);
            auto ipath = dpath + "/" + istr;
            if (!dhandle.exists(istr) || dhandle.childObjType(istr) != H5O_TYPE_GROUP) {
                throw std::runtime_error("expected a group at '" + ipath + "'");
            }
            auto lhandle = dhandle.openGroup(istr);
            lptr->set(i, parse_inner<Provisioner>(lhandle, ext, ipath, version));
        }

        if (named) {
            extract_names(handle, lptr, path, dpath);
        }

    } else if (object_type == "vector") {
        auto vector_type = load_string_attribute(handle, "uzuki_type", path);

        auto dpath = path + "/data";
        if (!handle.exists("data") || handle.childObjType("data") != H5O_TYPE_DATASET) {
            throw std::runtime_error("expected a dataset at '" + dpath + "'");
        }
        auto dhandle = handle.openDataSet("data");

        auto len = check_1d_length(dhandle, dpath, true);
        bool is_scalar = (len == 0);
        if (is_scalar) {
            len = 1;
        }

        bool named = handle.exists("names");

        if (vector_type == "integer") {
            auto iptr = Provisioner::new_Integer(len, named, is_scalar);
            output.reset(iptr);
            parse_integer_like(dhandle, iptr, dpath, [](int32_t) -> void {}, version);

        } else if (vector_type == "boolean") {
            auto bptr = Provisioner::new_Boolean(len, named, is_scalar);
            output.reset(bptr);
            parse_integer_like(dhandle, bptr, dpath, [&](int32_t x) -> void { 
                if (x != 0 && x != 1) {
                     throw std::runtime_error("boolean values should be 0 or 1 in '" + dpath + "'");
                }
            }, version);

        } else if (vector_type == "factor" || (version.equals(1, 0) && vector_type == "ordered")) {
            auto levpath = path + "/levels";
            if (!handle.exists("levels") || handle.childObjType("levels") != H5O_TYPE_DATASET) {
                throw std::runtime_error("expected a dataset at '" + levpath + "'");
            }
            auto levhandle = handle.openDataSet("levels");
            auto levtype = levhandle.getDataType();
            if (levtype.getClass() != H5T_STRING) {
                throw std::runtime_error("expected a string dataset at '" + levpath + "'");
            }
            int32_t levlen = check_1d_length(levhandle, levpath, false); // use int-32 for comparison with the integer codes.

            bool ordered = false;
            if (vector_type == "ordered") {
                ordered = true;
            } else if (handle.exists("ordered")) {
                auto ohandle = get_scalar_dataset(handle, "ordered", H5T_INTEGER, path);
                int tmp_ordered = 0;
                ohandle.read(&tmp_ordered, H5::PredType::NATIVE_INT);
                ordered = tmp_ordered > 0;
            }

            auto fptr = Provisioner::new_Factor(len, named, is_scalar, levlen, ordered);
            output.reset(fptr);
            parse_integer_like(dhandle, fptr, dpath, [&](int32_t x) -> void { 
                if (x < 0 || x >= levlen) {
                     throw std::runtime_error("factor codes should be non-negative and less than the number of levels in '" + dpath + "'");
                }
            }, version);

            std::unordered_set<std::string> present;
            load_string_dataset(levhandle, levlen, [&](size_t i, std::string x) -> void { 
                if (present.find(x) != present.end()) {
                    throw std::runtime_error("levels should be unique at '" + levpath + "'");
                }
                fptr->set_level(i, x); 
                present.insert(x);
            });

        } else if (vector_type == "string" || (version.equals(1, 0) && (vector_type == "date" || vector_type == "date-time"))) {
            StringVector::Format format = StringVector::NONE;
            if (version.equals(1, 0)) {
                if (vector_type == "date") {
                    format = StringVector::DATE;
                } else if (vector_type == "date-time") {
                    format = StringVector::DATETIME;
                }
            } else if (handle.exists("format")) {
                auto fhandle = get_scalar_dataset(handle, "format", H5T_STRING, path);
                load_string_dataset(fhandle, 1, [&](size_t, std::string x) -> void {
                    if (x == "date") {
                        format = StringVector::DATE;
                    } else if (x == "date-time") {
                        format = StringVector::DATETIME;
                    } else {
                        throw std::runtime_error("unsupported format '" + x + "' at '" + path + "/format'");
                    }
                });
            }

            auto sptr = Provisioner::new_String(len, named, is_scalar, format);
            output.reset(sptr);
            if (format == StringVector::NONE) {
                parse_string_like(dhandle, sptr, dpath, [](const std::string&) -> void {});
            } else if (format == StringVector::DATE) {
                parse_string_like(dhandle, sptr, dpath, [&](const std::string& x) -> void {
                    if (!is_date(x)) {
                         throw std::runtime_error("dates should follow YYYY-MM-DD formatting in '" + path + ".values'");
                    }
                });
            } else if (format == StringVector::DATETIME) {
                parse_string_like(dhandle, sptr, dpath, [&](const std::string& x) -> void {
                    if (!is_rfc3339(x)) {
                         throw std::runtime_error("date-times should follow the Internet Date/Time format in '" + path + ".values'");
                    }
                });
            }

        } else if (vector_type == "number") {
            auto dptr = Provisioner::new_Number(len, named, is_scalar);
            output.reset(dptr);
            parse_numbers(dhandle, dptr, dpath, [](double) -> void {}, version);

        } else {
            throw std::runtime_error("unknown vector type '" + vector_type + "' for '" + path + "'");
        }

        if (named) {
            auto vptr = static_cast<Vector*>(output.get());
            extract_names(handle, vptr, path, dpath);
        }

    } else if (object_type == "nothing") {
        output.reset(Provisioner::new_Nothing());

    } else if (object_type == "external") {
        auto ipath = path + "/index";
        if (!handle.exists("index") || handle.childObjType("index") != H5O_TYPE_DATASET) {
            throw std::runtime_error("expected a dataset at '" + ipath + "'");
        }

        auto ihandle = handle.openDataSet("index");
        if (ihandle.getDataType().getClass() != H5T_INTEGER) {
            throw std::runtime_error("expected integer dataset at '" + ipath + "'");
        }
        forbid_large_integers(ihandle, ipath);

        auto ispace = ihandle.getSpace();
        int idims = ispace.getSimpleExtentNdims();
        if (idims != 0) {
            throw std::runtime_error("expected scalar dataset at '" + ipath + "'");
        } 

        int idx;
        ihandle.read(&idx, H5::PredType::NATIVE_INT);
        if (idx < 0 || static_cast<size_t>(idx) >= ext.size()) {
            throw std::runtime_error("external index out of range at '" + ipath + "'");
        }

        output.reset(Provisioner::new_External(ext.get(idx)));

    } else {
        throw std::runtime_error("unknown uzuki2 object type '" + object_type + "'");
    }

    return output;
}
/**
 * @endcond
 */

/**
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * @tparam Externals Class describing how to resolve external references for type `EXTERNAL`.
 *
 * @param handle Handle for a HDF5 group corresponding to the list.
 * @param name Name of the HDF5 group corresponding to `handle`. 
 * Only used for error messages.
 * @param ext Instance of an external reference resolver class.
 *
 * @return Pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects. 
 * 
 * Any invalid representations in `contents` will cause an error to be thrown.
 *
 * @section provisioner-contract Provisioner requirements
 * The `Provisioner` class is expected to provide the following static methods:
 *
 * - `Nothing* new_Nothing()`, which returns a new instance of a `Nothing` subclass.
 * - `Other* new_Other(void* p)`, which returns a new instance of a `Other` subclass.
 *   `p` is a pointer to an "external" object, generated by calling `ext.get()` (see below).
 * - `List* new_List(size_t l, bool n)`, which returns a new instance of a `List` with length `l`.
 *   If `n = true`, names are present and will be added via `List::set_name()`.
 * - `IntegerVector* new_Integer(size_t l, bool n, bool s)`, which returns a new instance of an `IntegerVector` subclass of length `l`.
 *   If `n = true`, names are present and will be added via `Vector::set_name()`.
 *   If `s = true` and `l = 1`, the value was represented on file as a scalar integer.
 * - `NumberVector* new_Number(size_t l, bool n, bool s)`, which returns a new instance of a `NumberVector` subclass of length `l`.
 *   If `n = true`, names are present and will be added via `Vector::set_name()`.
 *   If `s = true` and `l = 1`, the value was represented on file as a scalar float.
 * - `StringVector* new_String(size_t l, bool n, bool s, StringVector::Format f)`, which returns a new instance of a `StringVector` subclass of length `l` with format `f`.
 *   If `n = true`, names are present and will be added via `Vector::set_name()`.
 *   If `s = true` and `l = 1`, the value was represented on file as a scalar string.
 * - `BooleanVector* new_Boolean(size_t l, bool n, bool s)`, which returns a new instance of a `BooleanVector` subclass of length `l`.
 *   If `n = true`, names are present and will be added via `Vector::set_name()`.
 *   If `s = true` and `l = 1`, the value was represented on file as a scalar boolean.
 * - `Factor* new_Factor(size_t l, bool n, bool s, size_t ll, bool o)`, which returns a new instance of a `Factor` subclass of length `l` and with `ll` unique levels.
 *   If `n = true`, names are present and will be added via `Vector::set_name()`.
 *   If `s = true` and `l = 1`, the lone index was represented on file as a scalar integer.
 *   If `o = true`, the levels should be assumed to be sorted.
 *
 * @section external-contract Externals requirements
 * The `Externals` class is expected to provide the following `const` methods:
 *
 * - `void* get(size_t i) const`, which returns a pointer to an "external" object, given the index of that object.
 *   This will be stored in the corresponding `Other` subclass generated by `Provisioner::new_Other`.
 * - `size_t size()`, which returns the number of available external references.
 */
template<class Provisioner, class Externals>
std::shared_ptr<Base> parse(const H5::Group& handle, const std::string& name, Externals ext) {
    Version version;
    if (handle.attrExists("uzuki_version")) {
        auto ver_str = load_string_attribute(handle.openAttribute("uzuki_version"), "uzuki_version", name);
        version = parse_version_string(ver_str);
    }

    ExternalTracker etrack(std::move(ext));
    auto ptr = parse_inner<Provisioner>(handle, etrack, name, version);
    etrack.validate();
    return ptr;
}

/**
 * Parse HDF5 file contents using the **uzuki2** specification, given the group handle.
 * It is assumed that there are no external references.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 *
 * @param handle Handle for a HDF5 group corresponding to the list.
 * @param name Name of the HDF5 group corresponding to `handle`. 
 * Only used for error messages.
 *
 * @return Pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects. 
 * 
 * Any invalid representations in `contents` will cause an error to be thrown.
 */
template<class Provisioner>
std::shared_ptr<Base> parse(const H5::Group& handle, const std::string& name) {
    return parse<Provisioner>(handle, name, uzuki2::DummyExternals(0));
}

/**
 * Parse HDF5 file contents using the **uzuki2** specification, given the file path.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * @tparam Externals Class describing how to resolve external references for type `EXTERNAL`.
 *
 * @param file Path to a HDF5 file.
 * @param name Name of the HDF5 group containing the list in `file`.
 * @param ext Instance of an external reference resolver class.
 *
 * @return Pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects. 
 * 
 * Any invalid representations in `contents` will cause an error to be thrown.
 */
template<class Provisioner, class Externals>
std::shared_ptr<Base> parse(const std::string& file, const std::string& name, Externals ext) {
    H5::H5File handle(file, H5F_ACC_RDONLY);
    return parse<Provisioner>(handle.openGroup(name), name, std::move(ext));
}

/**
 * Parse HDF5 file contents using the **uzuki2** specification, given the file path.
 * It is assumed that there are no external references.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 *
 * @param file Path to a HDF5 file.
 * @param name Name of the HDF5 group containing the list in `file`.
 *
 * @return Pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects. 
 * 
 * Any invalid representations in `contents` will cause an error to be thrown.
 */
template<class Provisioner>
std::shared_ptr<Base> parse(const std::string& file, const std::string& name) {
    H5::H5File handle(file, H5F_ACC_RDONLY);
    return parse<Provisioner>(handle.openGroup(name), name, uzuki2::DummyExternals(0));
}

/**
 * Validate HDF5 file contents against the **uzuki2** specification, given the group handle.
 * Any invalid representations will cause an error to be thrown.
 *
 * @param handle Handle for a HDF5 group corresponding to the list.
 * @param name Name of the HDF5 group corresponding to `handle`. 
 * Only used for error messages.
 * @param num_external Expected number of external references. 
 */
inline void validate(const H5::Group& handle, const std::string& name, int num_external = 0) {
    DummyExternals ext(num_external);
    parse<DummyProvisioner>(handle, name, ext);
    return;
}

/**
 * Validate HDF5 file contents against the **uzuki2** specification, given the file path.
 * Any invalid representations will cause an error to be thrown.
 *
 * @param file Path to a HDF5 file.
 * @param name Name of the HDF5 group containing the list in `file`.
 * @param num_external Expected number of external references. 
 */
inline void validate(const std::string& file, const std::string& name, int num_external = 0) {
    DummyExternals ext(num_external);
    parse<DummyProvisioner>(file, name, ext);
    return;
}

}

}

#endif
