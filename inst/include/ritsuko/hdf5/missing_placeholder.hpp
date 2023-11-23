#ifndef RITSUKO_HDF5_MISSING_PLACEHOLDER_HPP
#define RITSUKO_HDF5_MISSING_PLACEHOLDER_HPP

#include "H5Cpp.h"
#include <string>

#include "as_numeric_datatype.hpp"
#include "load_attribute.hpp"
#include "get_1d_length.hpp"
#include "get_name.hpp"

/**
 * @file missing_placeholder.hpp
 * @brief Get the missing placeholder attribute.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Check the validity of a missing placeholder attribute on a dataset.
 * An error is raised if the attribute is not a scalar or has a different type (or type class, if `type_class_only_ = true`) to the dataset.
 *
 * @param dset Dataset handle.
 * @param attr Handle for the missing placeholder, typically as an attribute on `dset`.
 * @param type_class_only Whether to only require identical type classes for the placeholder.
 * If 0, this is false, and the types between `dset` and `attr` must be identical.
 * If 1, this is true, and `dset` and `attr` just need to have the same type class.
 * If -1 (default), this is true for all string types and false for all numeric types.
 */
inline void check_missing_placeholder_attribute(const H5::DataSet& dset, const H5::Attribute& attr, int type_class_only = -1) {
    if (!is_scalar(attr)) {
        throw std::runtime_error("expected the '" + get_name(attr) + "' attribute to be a scalar");
    }

    if (type_class_only == -1) {
        type_class_only = (dset.getTypeClass() == H5T_STRING);
    }

    if (type_class_only == 1) {
        if (attr.getTypeClass() != dset.getTypeClass()) {
            throw std::runtime_error("expected the '" + get_name(attr) + "' attribute to have the same type class as its dataset");
        }
    } else {
        if (attr.getDataType() != dset.getDataType()) {
            throw std::runtime_error("expected the '" + get_name(attr) + "' attribute to have the same type as its dataset");
        }
    }
}

/**
 * Check if a missing numeric placeholder attribute is present, and if so, open it and loads it value.
 * This will also call `check_missing_placeholder_attribute()` to validate the placeholder's properties.
 *
 * @tparam Type_ Type to use to store the data in memory, see `as_numeric_datatype()` for supported types.
 * @param handle Dataset handle.
 * @param attr_name Name of the attribute containing the missing value placeholder.
 * @return Pair containing (i) a boolean indicating whether the placeholder attribute was present, and (ii) the value of the placeholder if the first element is `true`.
 */
template<typename Type_>
std::pair<bool, Type_> open_and_load_optional_numeric_missing_placeholder(const H5::DataSet& handle, const char* attr_name) {
    std::pair<bool, Type_> output(false, 0);
    if (!handle.attrExists(attr_name)) {
        return output;
    }
    output.first = true;
    auto ahandle = handle.openAttribute(attr_name);
    check_missing_placeholder_attribute(handle, ahandle);
    ahandle.read(as_numeric_datatype<Type_>(), &(output.second));
    return output;
}

/**
 * Check if a missing string placeholder attribute is present, and if so, open it and loads it value.
 * This will also call `check_missing_placeholder_attribute()` to validate the placeholder's properties.
 *
 * @param handle Dataset handle.
 * @param attr_name Name of the attribute containing the missing value placeholder.
 * @return Pair containing (i) a boolean indicating whether the placeholder attribute was present, and (ii) the value of the placeholder if the first element is `true`.
 */
inline std::pair<bool, std::string> open_and_load_optional_string_missing_placeholder(const H5::DataSet& handle, const char* attr_name) {
    std::pair<bool, std::string> output(false, "");
    if (!handle.attrExists(attr_name)) {
        return output;
    }
    output.first = true;
    auto ahandle = handle.openAttribute(attr_name);
    check_missing_placeholder_attribute(handle, ahandle);
    output.second = load_scalar_string_attribute(ahandle);
    return output;
}

}

}

#endif
