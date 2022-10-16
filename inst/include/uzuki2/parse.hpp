#ifndef UZUKI2_PARSE_HPP
#define UZUKI2_PARSE_HPP

#include <memory>
#include <vector>
#include <cctype>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <unordered_set>

#include "H5Cpp.h"
#include "interfaces.hpp"
#include "Dummy.hpp"

namespace uzuki2 {

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
void load_string_dataset(const H5::DataSet& handle, hsize_t full_length, const std::string& path, Function fun) {
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

inline hsize_t check_1d_length(const H5::DataSet& handle, const std::string& path) {
    auto dspace = handle.getSpace();
    int ndims = dspace.getSimpleExtentNdims();
    if (ndims != 1) {
        throw std::runtime_error("expected a 1-dimensional dataset at '" + path + "'");
    }
    hsize_t dims;
    dspace.getSimpleExtentDims(&dims);
    return dims;
}

inline bool is_date(const std::string& val) {
    if (val.size() != 10) {
        return false;
    } 
    
    for (size_t p = 0; p < val.size(); ++p) {
        if (p == 4 || p == 7) {
            if (val[p] != '-') {
                return false;
            }
        } else {
            if (!std::isdigit(val[p])) {
                return false;
            }
        }
    }

    if (val[5] == '1') {
        if (val[6] > '2') {
            return false;
        }
    } else if (val[5] != '0') {
        return false;
    }

    if (val[8] == '3') {
        if (val[6] > '1') {
            return false;
        }
    } else if (val[8] > '3') {
        return false;
    }

    return true;
}

template<class Host, class Function>
void parse_integer_like(H5::DataSet handle, Host* ptr, const std::string& path, Function check) {
    if (handle.getDataType().getClass() != H5T_INTEGER) {
        throw std::runtime_error("expected an integer dataset at '" + path + "'");
    }

    size_t len = ptr->size();

    // TODO: loop in chunks to reduce memory usage.
    std::vector<int32_t> buffer(len);
    handle.read(buffer.data(), H5::PredType::NATIVE_INT32);
    constexpr int32_t missing_value = -2147483648;

    for (hsize_t i = 0; i < len; ++i) {
        auto current = buffer[i];
        if (current == missing_value) {
            ptr->set_missing(i);
        } else {
            check(current);
            ptr->set(i, current);
        }
    }
}

template<class Host, class Function>
void parse_string_like(H5::DataSet handle, Host* ptr, const std::string& path, Function check) {
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

    load_string_dataset(handle, ptr->size(), path, [&](size_t i, std::string x) -> void {
        if (has_missing && x == missing_val) {
            ptr->set_missing(i);
        } else {
            check(x);
            ptr->set(i, std::move(x));
        }
    });
}

template<class Host, class Function>
void parse_numbers(H5::DataSet handle, Host* ptr, const std::string& path, Function check) {
    if (handle.getDataType().getClass() != H5T_FLOAT) {
        throw std::runtime_error("expected a float dataset at '" + path + "'");
    }

    size_t len = ptr->size();

    // TODO: loop in chunks to reduce memory usage.
    std::vector<double> buffer(len);
    handle.read(buffer.data(), H5::PredType::NATIVE_DOUBLE);

    for (hsize_t i = 0; i < len; ++i) {
        auto current = buffer[i];
        check(current);
        ptr->set(i, current);
    }
}

template<class Host>
void parse_names(H5::Group handle, Host* ptr, const std::string& path, const std::string& dpath) {
    if (handle.exists("names")) {
        auto npath = path + "/names";
        if (handle.childObjType("names") != H5O_TYPE_DATASET) {
            throw std::runtime_error("expected a dataset at '" + npath + "'");
        }
        ptr->use_names();

        auto nhandle = handle.openDataSet("names");
        auto dtype = nhandle.getDataType();
        if (dtype.getClass() != H5T_STRING) {
            throw std::runtime_error("expected a string dataset at '" + npath + "'");
        }

        auto len = ptr->size();
        auto nlen = check_1d_length(nhandle, npath);
        if (nlen != len) {
            throw std::runtime_error("length of '" + npath + "' should be equal to length of '" + dpath + "'");
        }

        load_string_dataset(nhandle, nlen, npath, [&](size_t i, std::string x) -> void { ptr->set_name(i, x); });
    }
}

template<class Provisioner, class Externals>
std::shared_ptr<Base> parse_inner(const H5::Group& handle, Externals& ext, const std::string& path) {
    // Deciding what type we're dealing with.
    auto object_type = load_string_attribute(handle, "uzuki_object", path);
    std::shared_ptr<Base> output;

    if (object_type == "list") {
        auto dpath = path + "/data";
        if (!handle.exists("data") || handle.childObjType("data") != H5O_TYPE_GROUP) {
            throw std::runtime_error("expected a group at '" + dpath + "'");
        }
        auto dhandle = handle.openGroup("data");
        auto len = dhandle.getNumObjs();
        auto lptr = Provisioner::new_List(len);
        output.reset(lptr);

        for (int i = 0; i < len; ++i) {
            auto istr = std::to_string(i);
            auto ipath = dpath + "/" + istr;
            if (!dhandle.exists(istr) || dhandle.childObjType(istr) != H5O_TYPE_GROUP) {
                throw std::runtime_error("expected a group at '" + ipath + "'");
            }
            auto lhandle = dhandle.openGroup(istr);
            lptr->set(i, parse_inner<Provisioner>(lhandle, ext, ipath));
        }

        parse_names(handle, lptr, path, dpath);

    } else if (object_type == "vector") {
        auto vector_type = load_string_attribute(handle, "uzuki_type", path);

        auto dpath = path + "/data";
        if (!handle.exists("data") || handle.childObjType("data") != H5O_TYPE_DATASET) {
            throw std::runtime_error("expected a dataset at '" + dpath + "'");
        }
        auto dhandle = handle.openDataSet("data");
        auto len = check_1d_length(dhandle, dpath);

        if (vector_type == "integer") {
            auto iptr = Provisioner::new_Integer(len);
            output.reset(iptr);
            parse_integer_like(dhandle, iptr, dpath, [](int32_t x) -> void {});

        } else if (vector_type == "boolean") {
            auto bptr = Provisioner::new_Boolean(len);
            output.reset(bptr);
            parse_integer_like(dhandle, bptr, dpath, [&](int32_t x) -> void { 
                if (x != 0 && x != 1) {
                     throw std::runtime_error("boolean values should be 0 or 1 in '" + dpath + "'");
                }
            });

        } else if (vector_type == "factor" || vector_type == "ordered") {
            // First we need to figure out the number of levels.
            auto levpath = path + "/levels";
            if (!handle.exists("levels") || handle.childObjType("levels") != H5O_TYPE_DATASET) {
                throw std::runtime_error("expected a dataset at '" + levpath + "'");
            }
            auto levhandle = handle.openDataSet("levels");
            auto levtype = levhandle.getDataType();
            if (levtype.getClass() != H5T_STRING) {
                throw std::runtime_error("expected a string dataset at '" + levpath + "'");
            }
            auto levlen = check_1d_length(levhandle, levpath);

            // Then we can initialize the interface.
            auto fptr = Provisioner::new_Factor(len, levlen);
            output.reset(fptr);
            if (vector_type == "ordered") {
                fptr->is_ordered();
            }

            parse_integer_like(dhandle, fptr, dpath, [&](int32_t x) -> void { 
                if (x < 0 || x >= levlen) {
                     throw std::runtime_error("factor codes should be non-negative and less than the number of levels in '" + dpath + "'");
                }
            });

            std::unordered_set<std::string> present;
            load_string_dataset(levhandle, levlen, levpath, [&](size_t i, std::string x) -> void { 
                if (present.find(x) != present.end()) {
                    throw std::runtime_error("levels should be unique at '" + levpath + "'");
                }
                fptr->set_level(i, x); 
                present.insert(x);
            });

        } else if (vector_type == "string") {
            auto sptr = Provisioner::new_String(len);
            output.reset(sptr);
            parse_string_like(dhandle, sptr, dpath, [](const std::string& x) -> void {});

        } else if (vector_type == "date") {
            auto dptr = Provisioner::new_Date(len);
            output.reset(dptr);
            parse_string_like(dhandle, dptr, dpath, [&](const std::string& x) -> void {
                if (!is_date(x)) {
                     throw std::runtime_error("dates should follow YYYY-MM-DD formatting in '" + dpath + "'");
                }
            });

        } else if (vector_type == "number") {
            auto dptr = Provisioner::new_Number(len);
            output.reset(dptr);
            parse_numbers(dhandle, dptr, dpath, [](double x) -> void {});

        } else {
            throw std::runtime_error("unknown vector type '" + vector_type + "' for '" + path + "'");
        }

        auto vptr = static_cast<Vector*>(output.get());
        parse_names(handle, vptr, path, dpath);

    } else if (object_type == "nothing") {
        output.reset(Provisioner::new_Nothing());

    } else if (object_type == "external") {
        auto ipath = path + "/index";
        if (!handle.exists("index") || handle.childObjType("index") != H5O_TYPE_DATASET) {
            throw std::runtime_error("expected a dataset at '" + ipath + "'");
        }
        auto ihandle = handle.openDataSet("index");

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

template<class CustomExternals>
struct ExternalTracker {
    ExternalTracker(CustomExternals e) : getter(std::move(e)) {}

    void* get(size_t i) {
        indices.push_back(i);
        return getter.get(i);
    };

    size_t size() const {
        return getter.size();
    }

    CustomExternals getter;
    std::vector<size_t> indices;
};
/**
 * @endcond
 */

/**
 * Parse HDF5 file contents using the **uzuki2** specification, given the group handle.
 *
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
 * - `List* new_List(size_t l)`, which returns a new instance of a `List` with length `l`.
 * - `IntegerVector* new_Integer(size_t l)`, which returns a new instance of an `IntegerVector` subclass of length `l`.
 * - `NumberVector* new_Number(size_t l)`, which returns a new instance of a `NumberVector` subclass of length `l`.
 * - `StringVector* new_String(size_t l)`, which returns a new instance of a `StringVector` subclass of length `l`.
 * - `BooleanVector* new_Boolean(size_t l)`, which returns a new instance of a `BooleanVector` subclass of length `l`.
 * - `DateVector* new_Date(size_t l)`, which returns a new instance of a `DateVector` subclass of length `l`.
 * - `Factor* new_Factor(size_t l, size_t ll)`, which returns a new instance of a `Factor` subclass of length `l` and with `ll` unique levels.

 * @section external-contract Externals requirements
 * The `Externals` class is expected to provide the following `const` methods:
 *
 * - `void* get(size_t i) const`, which returns a pointer to an "external" object, given the index of that object.
 *   This will be stored in the corresponding `Other` subclass generated by `Provisioner::new_Other`.
 * - `size_t size()`, which returns the number of available external references.
 */
template<class Provisioner, class Externals>
std::shared_ptr<Base> parse(const H5::Group& handle, const std::string& name, Externals ext) {
    ExternalTracker etrack(std::move(ext));
    auto ptr = parse_inner<Provisioner>(handle, etrack, name);

    // Checking that the external indices match up.
    auto& other_indices = etrack.indices;
    if (other_indices.size() != etrack.getter.size()) {
        throw std::runtime_error("fewer instances of type \"external\" than expected from 'ext'");
    }

    std::sort(other_indices.begin(), other_indices.end());
    for (int i = 0; i < static_cast<int>(other_indices.size()); ++i) {
        if (i != other_indices[i]) {
            throw std::runtime_error("set of \"index\" values for type \"external\" should be consecutive starting from zero");
        }
    }

    return ptr;
}

/**
 * Parse HDF5 file contents using the **uzuki2** specification, given the file name.
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
 * Parse HDF5 file contents using the **uzuki2** specification, given the file name.
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

}

#endif
