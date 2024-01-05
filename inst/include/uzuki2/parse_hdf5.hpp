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
inline H5::DataSet check_scalar_dataset(const H5::Group& handle, const char* name) {
    if (handle.childObjType(name) != H5O_TYPE_DATASET) {
        throw std::runtime_error("expected '" + std::string(name) + "' to be a dataset");
    }
    auto dhandle = handle.openDataSet(name);
    if (!ritsuko::hdf5::is_scalar(dhandle)) {
        throw std::runtime_error("expected '" + std::string(name) + "'to be a scalar dataset");
    }
    return dhandle;
}

template<class Host, class Function>
void parse_integer_like(const H5::DataSet& handle, Host* ptr, Function check, const Version& version, hsize_t buffer_size) try {
    if (ritsuko::hdf5::exceeds_integer_limit(handle, 32, true)) {
        throw std::runtime_error("dataset cannot be represented by 32-bit signed integers");
    }

    bool has_missing = false;
    int32_t missing_value = -2147483648;
    if (version.equals(1, 0)) {
        has_missing = true;
    } else {
        const char* placeholder_name = "missing-value-placeholder";
        has_missing = handle.attrExists(placeholder_name);
        if (has_missing) {
            auto attr = handle.openAttribute(placeholder_name);
            ritsuko::hdf5::check_missing_placeholder_attribute(handle, attr, /* type_class_only = */ version.lt(1, 2));
            attr.read(H5::PredType::NATIVE_INT32, &missing_value);
        }
    }

    hsize_t full_length = ptr->size();
    ritsuko::hdf5::Stream1dNumericDataset<int32_t> stream(&handle, full_length, buffer_size);
    for (hsize_t i = 0; i < full_length; ++i, stream.next()) {
        auto current = stream.get();
        if (has_missing && current == missing_value) {
            ptr->set_missing(i);
        } else {
            check(current);
            ptr->set(i, current);
        }
    }

} catch (std::exception& e) {
    throw std::runtime_error("failed to load integer dataset at '" + ritsuko::hdf5::get_name(handle) + "'; " + std::string(e.what()));
}

template<class Host, class Function>
void parse_string_like(const H5::DataSet& handle, Host* ptr, Function check, hsize_t buffer_size) try {
    auto dtype = handle.getDataType();
    if (dtype.getClass() != H5T_STRING) {
        throw std::runtime_error("expected a string dataset");
    }

    auto missingness = ritsuko::hdf5::open_and_load_optional_string_missing_placeholder(handle, "missing-value-placeholder");
    bool has_missing = missingness.first;
    std::string missing_val = missingness.second;

    hsize_t full_length = ptr->size();
    ritsuko::hdf5::Stream1dStringDataset stream(&handle, full_length, buffer_size);
    for (hsize_t i = 0; i < full_length; ++i, stream.next()) {
        auto x = stream.steal();
        if (has_missing && x == missing_val) {
            ptr->set_missing(i);
        } else {
            check(x);
            ptr->set(i, std::move(x));
        }
    }

} catch (std::exception& e) {
    throw std::runtime_error("failed to load string dataset at '" + ritsuko::hdf5::get_name(handle) + "'; " + std::string(e.what()));
}

template<class Host, class Function>
void parse_numbers(const H5::DataSet& handle, Host* ptr, Function check, const Version& version, hsize_t buffer_size) try {
    if (version.lt(1, 3)) {
        if (handle.getTypeClass() != H5T_FLOAT) {
            throw std::runtime_error("expected a floating-point dataset");
        }
    } else {
        if (ritsuko::hdf5::exceeds_float_limit(handle, 64)) {
            throw std::runtime_error("dataset cannot be represented by 64-bit floats");
        }
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
            auto attr = handle.openAttribute(placeholder_name);
            ritsuko::hdf5::check_missing_placeholder_attribute(handle, attr, /* type_class_only = */ version.lt(1, 2));
            attr.read(H5::PredType::NATIVE_DOUBLE, &missing_value);
        }
    }

    bool should_compare_nan = version.lt(1, 3);
    bool is_placeholder_nan = std::isnan(missing_value);
    auto is_missing_value = [&](double val) -> bool {
        if (should_compare_nan) {
            return ritsuko::are_floats_identical(&val, &missing_value);
        } else if (is_placeholder_nan) {
            return std::isnan(val);
        } else {
            return val == missing_value;
        }
    };

    hsize_t full_length = ptr->size();
    ritsuko::hdf5::Stream1dNumericDataset<double> stream(&handle, full_length, buffer_size);
    for (hsize_t i = 0; i < full_length; ++i, stream.next()) {
        auto current = stream.get();
        if (has_missing && is_missing_value(current)) {
            ptr->set_missing(i);
        } else {
            check(current);
            ptr->set(i, current);
        }
    }

} catch (std::exception& e) {
    throw std::runtime_error("failed to load floating-point dataset at '" + ritsuko::hdf5::get_name(handle) + "'; " + std::string(e.what()));
}

template<class Host>
void extract_names(const H5::Group& handle, Host* ptr, hsize_t buffer_size) try {
    if (handle.childObjType("names") != H5O_TYPE_DATASET) {
        throw std::runtime_error("expected a dataset");
    }

    auto nhandle = handle.openDataSet("names");
    auto dtype = nhandle.getDataType();
    if (dtype.getClass() != H5T_STRING) {
        throw std::runtime_error("expected a string dataset");
    }

    size_t len = ptr->size();
    size_t nlen = ritsuko::hdf5::get_1d_length(nhandle.getSpace(), false);
    if (nlen != len) {
        throw std::runtime_error("number of names should be equal to the object length");
    }

    ritsuko::hdf5::Stream1dStringDataset stream(&nhandle, nlen, buffer_size);
    for (size_t i = 0; i < nlen; ++i, stream.next()) {
        ptr->set_name(i, stream.steal());
    }
} catch (std::exception& e) {
    throw std::runtime_error("failed to load names at '" + ritsuko::hdf5::get_name(handle) + "'; " + std::string(e.what()));
}

template<class Provisioner, class Externals>
std::shared_ptr<Base> parse_inner(const H5::Group& handle, Externals& ext, const Version& version, hsize_t buffer_size) try {
    // Deciding what type we're dealing with.
    auto object_type = ritsuko::hdf5::open_and_load_scalar_string_attribute(handle, "uzuki_object");
    std::shared_ptr<Base> output;

    if (object_type == "list") {
        auto dhandle = ritsuko::hdf5::open_group(handle, "data");
        size_t len = dhandle.getNumObjs();

        bool named = handle.exists("names");
        auto lptr = Provisioner::new_List(len, named);
        output.reset(lptr);

        try {
            for (size_t i = 0; i < len; ++i) {
                auto istr = std::to_string(i);
                auto lhandle = ritsuko::hdf5::open_group(dhandle, istr.c_str());
                lptr->set(i, parse_inner<Provisioner>(lhandle, ext, version, buffer_size));
            }
        } catch (std::exception& e) {
            throw std::runtime_error("failed to parse list contents in 'data'; " + std::string(e.what()));
        }

        if (named) {
            extract_names(handle, lptr, buffer_size);
        }

    } else if (object_type == "vector") {
        auto vector_type = ritsuko::hdf5::open_and_load_scalar_string_attribute(handle, "uzuki_type");

        auto dhandle = ritsuko::hdf5::open_dataset(handle, "data");
        size_t len = ritsuko::hdf5::get_1d_length(dhandle.getSpace(), true);
        bool is_scalar = (len == 0);
        if (is_scalar) {
            len = 1;
        }

        bool named = handle.exists("names");

        if (vector_type == "integer") {
            auto iptr = Provisioner::new_Integer(len, named, is_scalar);
            output.reset(iptr);
            parse_integer_like(dhandle, iptr, [](int32_t) -> void {}, version, buffer_size);

        } else if (vector_type == "boolean") {
            auto bptr = Provisioner::new_Boolean(len, named, is_scalar);
            output.reset(bptr);
            parse_integer_like(dhandle, bptr, [&](int32_t x) -> void { 
                if (x != 0 && x != 1) {
                     throw std::runtime_error("boolean values should be 0 or 1");
                }
            }, version, buffer_size);

        } else if (vector_type == "factor" || (version.equals(1, 0) && vector_type == "ordered")) {
            auto levhandle = ritsuko::hdf5::open_dataset(handle, "levels");
            auto levtype = levhandle.getDataType();
            if (levtype.getClass() != H5T_STRING) {
                throw std::runtime_error("expected a string dataset for the levels at 'levels'");
            }

            int32_t levlen = ritsuko::hdf5::get_1d_length(levhandle.getSpace(), false);
            bool ordered = false;
            if (vector_type == "ordered") {
                ordered = true;
            } else if (handle.exists("ordered")) {
                auto ohandle = check_scalar_dataset(handle, "ordered");
                if (ritsuko::hdf5::exceeds_integer_limit(ohandle, 32, true)) {
                    throw std::runtime_error("'ordered' value cannot be represented by a 32-bit integer");
                }
                int32_t tmp_ordered = 0;
                ohandle.read(&tmp_ordered, H5::PredType::NATIVE_INT32);
                ordered = tmp_ordered > 0;
            }

            auto fptr = Provisioner::new_Factor(len, named, is_scalar, levlen, ordered);
            output.reset(fptr);
            parse_integer_like(dhandle, fptr, [&](int32_t x) -> void { 
                if (x < 0 || x >= levlen) {
                     throw std::runtime_error("factor codes should be non-negative and less than the number of levels");
                }
            }, version, buffer_size);

            std::unordered_set<std::string> present;
            ritsuko::hdf5::Stream1dStringDataset stream(&levhandle, levlen, buffer_size);
            for (int32_t i = 0; i < levlen; ++i, stream.next()) {
                auto x = stream.steal();
                if (present.find(x) != present.end()) {
                    throw std::runtime_error("levels should be unique");
                }
                fptr->set_level(i, x); 
                present.insert(std::move(x));
            }

        } else if (vector_type == "string" || (version.equals(1, 0) && (vector_type == "date" || vector_type == "date-time"))) {
            StringVector::Format format = StringVector::NONE;
            if (version.equals(1, 0)) {
                if (vector_type == "date") {
                    format = StringVector::DATE;
                } else if (vector_type == "date-time") {
                    format = StringVector::DATETIME;
                }

            } else if (handle.exists("format")) {
                auto fhandle = check_scalar_dataset(handle, "format");
                if (fhandle.getTypeClass() != H5T_STRING) {
                    throw std::runtime_error("'format' dataset should have a string datatype class");
                }
                auto x = ritsuko::hdf5::load_scalar_string_dataset(fhandle);
                if (x == "date") {
                    format = StringVector::DATE;
                } else if (x == "date-time") {
                    format = StringVector::DATETIME;
                } else {
                    throw std::runtime_error("unsupported format '" + x + "'");
                }
            }

            auto sptr = Provisioner::new_String(len, named, is_scalar, format);
            output.reset(sptr);
            if (format == StringVector::NONE) {
                parse_string_like(dhandle, sptr, [](const std::string&) -> void {}, buffer_size);

            } else if (format == StringVector::DATE) {
                parse_string_like(dhandle, sptr, [&](const std::string& x) -> void {
                    if (!ritsuko::is_date(x.c_str(), x.size())) {
                         throw std::runtime_error("dates should follow YYYY-MM-DD formatting");
                    }
                }, buffer_size);

            } else if (format == StringVector::DATETIME) {
                parse_string_like(dhandle, sptr, [&](const std::string& x) -> void {
                    if (!ritsuko::is_rfc3339(x.c_str(), x.size())) {
                         throw std::runtime_error("date-times should follow the Internet Date/Time format");
                    }
                }, buffer_size);
            }

        } else if (vector_type == "number") {
            auto dptr = Provisioner::new_Number(len, named, is_scalar);
            output.reset(dptr);
            parse_numbers(dhandle, dptr, [](double) -> void {}, version, buffer_size);

        } else {
            throw std::runtime_error("unknown vector type '" + vector_type + "'");
        }

        if (named) {
            auto vptr = static_cast<Vector*>(output.get());
            extract_names(handle, vptr, buffer_size);
        }

    } else if (object_type == "nothing") {
        output.reset(Provisioner::new_Nothing());

    } else if (object_type == "external") {
        auto ihandle = ritsuko::hdf5::open_dataset(handle, "index");
        if (ritsuko::hdf5::exceeds_integer_limit(ihandle, 32, true)) {
            throw std::runtime_error("external index at 'index' cannot be represented by a 32-bit signed integer");
        }

        auto ispace = ihandle.getSpace();
        int idims = ispace.getSimpleExtentNdims();
        if (idims != 0) {
            throw std::runtime_error("expected scalar dataset at 'index'");
        } 

        int32_t idx;
        ihandle.read(&idx, H5::PredType::NATIVE_INT32);
        if (idx < 0 || static_cast<size_t>(idx) >= ext.size()) {
            throw std::runtime_error("external index out of range at 'index'");
        }

        output.reset(Provisioner::new_External(ext.get(idx)));

    } else {
        throw std::runtime_error("unknown uzuki2 object type '" + object_type + "'");
    }

    return output;
} catch (std::exception& e) {
    throw std::runtime_error("failed to load object at '" + ritsuko::hdf5::get_name(handle) + "'; " + std::string(e.what()));
}
/**
 * @endcond
 */

/**
 * @brief Options for HDF5 file parsing.
 */
struct Options {
    /**
     * Buffer size, in terms of the number of elements, to use for reading data from HDF5 datasets.
     */
    hsize_t buffer_size = 10000;

    /**
     * Whether to throw an error if the top-level R object is not an R list.
     */
    bool strict_list = true;
};

/**
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 * @tparam Externals Class describing how to resolve external references for type `EXTERNAL`.
 *
 * @param handle Handle for a HDF5 group corresponding to the list.
 * @param ext Instance of an external reference resolver class.
 * @param options Optional parameters.
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
ParsedList parse(const H5::Group& handle, Externals ext, Options options = Options()) {
    Version version;
    if (handle.attrExists("uzuki_version")) {
        auto ver_str = ritsuko::hdf5::open_and_load_scalar_string_attribute(handle, "uzuki_version");
        auto vraw = ritsuko::parse_version_string(ver_str.c_str(), ver_str.size(), /* skip_patch = */ true);
        version.major = vraw.major;
        version.minor = vraw.minor;
    }

    ExternalTracker etrack(std::move(ext));
    auto ptr = parse_inner<Provisioner>(handle, etrack, version, options.buffer_size);

    if (options.strict_list && ptr->type() != LIST) {
        throw std::runtime_error("top-level object should represent an R list");
    }
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
 * @param options Optional parameters.
 *
 * @return A `ParsedList` containing a pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects. 
 * 
 * Any invalid representations in `contents` will cause an error to be thrown.
 */
template<class Provisioner>
ParsedList parse(const H5::Group& handle, Options options = Options()) {
    return parse<Provisioner>(handle, uzuki2::DummyExternals(0), std::move(options));
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
 * @param options Optional parameters.
 *
 * @return A `ParsedList` containing a pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects. 
 * 
 * Any invalid representations in `contents` will cause an error to be thrown.
 */
template<class Provisioner, class Externals>
ParsedList parse(const std::string& file, const std::string& name, Externals ext, Options options = Options()) {
    H5::H5File handle(file, H5F_ACC_RDONLY);
    return parse<Provisioner>(ritsuko::hdf5::open_group(handle, name.c_str()), std::move(ext), std::move(options));
}

/**
 * Parse HDF5 file contents using the **uzuki2** specification, given the file path.
 * It is assumed that there are no external references.
 *
 * @tparam Provisioner A class namespace defining static methods for creating new `Base` objects.
 *
 * @param file Path to a HDF5 file.
 * @param name Name of the HDF5 group containing the list in `file`.
 * @param options Optional parameters.
 *
 * @return A `ParsedList` containing a pointer to the root `Base` object.
 * Depending on `Provisioner`, this may contain references to all nested objects. 
 * 
 * Any invalid representations in `contents` will cause an error to be thrown.
 */
template<class Provisioner>
ParsedList parse(const std::string& file, const std::string& name, Options options = Options()) {
    H5::H5File handle(file, H5F_ACC_RDONLY);
    return parse<Provisioner>(ritsuko::hdf5::open_group(handle, name.c_str()), uzuki2::DummyExternals(0), std::move(options));
}

/**
 * Validate HDF5 file contents against the **uzuki2** specification, given the group handle.
 * Any invalid representations will cause an error to be thrown.
 *
 * @param handle Handle for a HDF5 group corresponding to the list.
 * @param num_external Expected number of external references. 
 * @param options Optional parameters.
 */
inline void validate(const H5::Group& handle, int num_external = 0, Options options = Options()) {
    DummyExternals ext(num_external);
    parse<DummyProvisioner>(handle, ext, std::move(options));
    return;
}

/**
 * Validate HDF5 file contents against the **uzuki2** specification, given the file path.
 * Any invalid representations will cause an error to be thrown.
 *
 * @param file Path to a HDF5 file.
 * @param name Name of the HDF5 group containing the list in `file`.
 * @param num_external Expected number of external references. 
 * @param options Optional parameters.
 */
inline void validate(const std::string& file, const std::string& name, int num_external = 0, Options options = Options()) {
    DummyExternals ext(num_external);
    parse<DummyProvisioner>(file, name, ext, std::move(options));
    return;
}

}

}

#endif
