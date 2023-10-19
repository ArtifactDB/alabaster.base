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
#include "ExternalTracker.hpp"
#include "Version.hpp"
#include "ParsedList.hpp"

#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

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
inline H5::DataSet get_scalar_dataset(const H5::Group& handle, const std::string& name, H5T_class_t type_class, const std::string& path) {
    auto dhandle = ritsuko::hdf5::get_scalar_dataset(handle, name.c_str());
    if (dhandle.getTypeClass() != type_class) {
        throw std::runtime_error("dataset at '" + path + "/" + name + "' has the wrong datatype class");
    }
    return dhandle;
}

template<class Host, class Function>
void parse_integer_like(const H5::DataSet& handle, Host* ptr, const std::string& path, Function check, const Version& version) {
    if (handle.getDataType().getClass() != H5T_INTEGER) {
        throw std::runtime_error("expected an integer dataset at '" + path + "'");
    }
    ritsuko::hdf5::forbid_large_integers(handle, 32, path.c_str());

    bool has_missing = false;
    int32_t missing_value = -2147483648;
    if (version.equals(1, 0)) {
        has_missing = true;
    } else {
        const char* placeholder_name = "missing-value-placeholder";
        has_missing = handle.attrExists(placeholder_name);
        if (has_missing) {
            auto attr = ritsuko::hdf5::get_missing_placeholder_attribute(handle, placeholder_name, path.c_str(), /* type_class_only = */ version.lt(1, 2));
            attr.read(H5::PredType::NATIVE_INT32, &missing_value);
        }
    }

    hsize_t full_length = ptr->size();
    auto block_size = ritsuko::hdf5::pick_1d_block_size(handle.getCreatePlist(), full_length, /* buffer_size = */ 10000);
    std::vector<int32_t> buffer(block_size);
    ritsuko::hdf5::iterate_1d_blocks(
        full_length,
        block_size, 
        [&](hsize_t counter, hsize_t limit, const H5::DataSpace& mspace, const H5::DataSpace& dspace) -> void {
            handle.read(buffer.data(), H5::PredType::NATIVE_INT32, mspace, dspace);
            for (hsize_t i = 0; i < limit; ++i) {
                auto current = buffer[i];
                if (has_missing && current == missing_value) {
                    ptr->set_missing(counter + i);
                } else {
                    check(current);
                    ptr->set(counter + i, current);
                }
            }
        }
    );
}

template<class Host, class Function>
void parse_string_like(const H5::DataSet& handle, Host* ptr, const std::string& path, Function check) {
    auto dtype = handle.getDataType();
    if (dtype.getClass() != H5T_STRING) {
        throw std::runtime_error("expected a string dataset at '" + path + "'");
    }

    const char* placeholder_name = "missing-value-placeholder";
    bool has_missing = handle.attrExists(placeholder_name);
    std::string missing_val;
    if (has_missing) {
        auto ahandle = ritsuko::hdf5::get_missing_placeholder_attribute(handle, placeholder_name, path.c_str(), /* type_class_only = */ true);
        missing_val = ritsuko::hdf5::load_scalar_string_attribute(ahandle, placeholder_name, path.c_str());
    }

    ritsuko::hdf5::load_1d_string_dataset(
        handle, 
        ptr->size(), 
        /* buffer_size = */ 10000,
        [&](size_t i, const char* str, size_t len) -> void {
            std::string x(str, str + len);
            if (has_missing && x == missing_val) {
                ptr->set_missing(i);
            } else {
                check(x);
                ptr->set(i, std::move(x));
            }
        }
    );
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
        missing_value = ritsuko::r_missing_value();
    } else {
        const char* placeholder_name = "missing-value-placeholder";
        has_missing = handle.attrExists(placeholder_name);
        if (has_missing) {
            auto attr = ritsuko::hdf5::get_missing_placeholder_attribute(handle, placeholder_name, path.c_str(), /* type_class_only = */ version.lt(1, 2));
            attr.read(H5::PredType::NATIVE_DOUBLE, &missing_value);
        }
    }

    hsize_t full_length = ptr->size();
    auto block_size = ritsuko::hdf5::pick_1d_block_size(handle.getCreatePlist(), full_length, /* buffer_size = */ 10000);
    std::vector<double> buffer(block_size);
    ritsuko::hdf5::iterate_1d_blocks(
        full_length,
        block_size, 
        [&](hsize_t counter, hsize_t limit, const H5::DataSpace& mspace, const H5::DataSpace& dspace) -> void {
            handle.read(buffer.data(), H5::PredType::NATIVE_DOUBLE, mspace, dspace);
            for (hsize_t i = 0; i < limit; ++i) {
                auto current = buffer[i];
                if (has_missing && ritsuko::are_floats_identical(&current, &missing_value)) {
                    ptr->set_missing(counter + i);
                } else {
                    check(current);
                    ptr->set(counter + i, current);
                }
            }
        }
    ); 
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
    auto nlen = ritsuko::hdf5::get_1d_length(nhandle.getSpace(), false, npath.c_str());
    if (nlen != len) {
        throw std::runtime_error("length of '" + npath + "' should be equal to length of '" + dpath + "'");
    }

    ritsuko::hdf5::load_1d_string_dataset(
        nhandle, 
        nlen, 
        /* buffer_size = */ 10000,
        [&](size_t i, const char* val, size_t len) -> void { 
            ptr->set_name(i, std::string(val, val + len));
        }
    );
}

template<class Provisioner, class Externals>
std::shared_ptr<Base> parse_inner(const H5::Group& handle, Externals& ext, const std::string& path, const Version& version) {
    // Deciding what type we're dealing with.
    auto object_type = ritsuko::hdf5::load_scalar_string_attribute(handle, "uzuki_object", path.c_str());
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
        auto vector_type = ritsuko::hdf5::load_scalar_string_attribute(handle, "uzuki_type", path.c_str());

        auto dhandle = ritsuko::hdf5::get_dataset(handle, "data");
        std::string dpath = path + "/data";
        auto len = ritsuko::hdf5::get_1d_length(dhandle.getSpace(), true, dpath.c_str());
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
            auto levhandle = ritsuko::hdf5::get_dataset(handle, "levels");
            auto levtype = levhandle.getDataType();
            std::string levpath = path + "/levels";
            if (levtype.getClass() != H5T_STRING) {
                throw std::runtime_error("expected a string dataset at '" + levpath + "'");
            }
            int32_t levlen = ritsuko::hdf5::get_1d_length(levhandle.getSpace(), false, levpath.c_str()); // use int-32 for comparison with the integer codes.

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
            ritsuko::hdf5::load_1d_string_dataset(
                levhandle, 
                levlen, 
                /* buffer_size = */ 10000,
                [&](size_t i, const char* val, size_t len) -> void { 
                    std::string x(val, val + len);
                    if (present.find(x) != present.end()) {
                        throw std::runtime_error("levels should be unique at '" + path + "/levels'");
                    }
                    fptr->set_level(i, x); 
                    present.insert(std::move(x));
                }
            );

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
                ritsuko::hdf5::load_1d_string_dataset(
                    fhandle, 
                    1, 
                    /* buffer_size = */ 10000,
                    [&](size_t, const char* val, size_t len) -> void {
                        std::string x(val, val + len);
                        if (x == "date") {
                            format = StringVector::DATE;
                        } else if (x == "date-time") {
                            format = StringVector::DATETIME;
                        } else {
                            throw std::runtime_error("unsupported format '" + x + "' at '" + path + "/format'");
                        }
                    }
                );
            }

            auto sptr = Provisioner::new_String(len, named, is_scalar, format);
            output.reset(sptr);
            if (format == StringVector::NONE) {
                parse_string_like(dhandle, sptr, dpath, [](const std::string&) -> void {});

            } else if (format == StringVector::DATE) {
                parse_string_like(dhandle, sptr, dpath, [&](const std::string& x) -> void {
                    if (!ritsuko::is_date(x.c_str(), x.size())) {
                         throw std::runtime_error("dates should follow YYYY-MM-DD formatting in '" + path + ".values'");
                    }
                });

            } else if (format == StringVector::DATETIME) {
                parse_string_like(dhandle, sptr, dpath, [&](const std::string& x) -> void {
                    if (!ritsuko::is_rfc3339(x.c_str(), x.size())) {
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
        auto ihandle = ritsuko::hdf5::get_dataset(handle, "index");
        if (ihandle.getDataType().getClass() != H5T_INTEGER) {
            throw std::runtime_error("expected integer dataset at '" + ipath + "'");
        }
        ritsuko::hdf5::forbid_large_integers(ihandle, 32, ipath.c_str());

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
 * @return A `ParsedList` containing a pointer to the root `Base` object.
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
ParsedList parse(const H5::Group& handle, const std::string& name, Externals ext) {
    Version version;
    if (handle.attrExists("uzuki_version")) {
        auto ver_str = ritsuko::hdf5::load_scalar_string_attribute(handle.openAttribute("uzuki_version"), "uzuki_version", name.c_str());
        version = parse_version_string(ver_str);
    }

    ExternalTracker etrack(std::move(ext));
    auto ptr = parse_inner<Provisioner>(handle, etrack, name, version);
    etrack.validate();
    return ParsedList(std::move(ptr), std::move(version));
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
 * @return A `ParsedList` containing a pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects. 
 * 
 * Any invalid representations in `contents` will cause an error to be thrown.
 */
template<class Provisioner>
ParsedList parse(const H5::Group& handle, const std::string& name) {
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
 * @return A `ParsedList` containing a pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects. 
 * 
 * Any invalid representations in `contents` will cause an error to be thrown.
 */
template<class Provisioner, class Externals>
ParsedList parse(const std::string& file, const std::string& name, Externals ext) {
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
 * @return A `ParsedList` containing a pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects. 
 * 
 * Any invalid representations in `contents` will cause an error to be thrown.
 */
template<class Provisioner>
ParsedList parse(const std::string& file, const std::string& name) {
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
