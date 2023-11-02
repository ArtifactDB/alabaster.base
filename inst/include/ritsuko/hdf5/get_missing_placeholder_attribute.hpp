#ifndef RITSUKO_HDF5_GET_MISSING_PLACEHOLDER_ATTRIBUTE_HPP
#define RITSUKO_HDF5_GET_MISSING_PLACEHOLDER_ATTRIBUTE_HPP

#include "H5Cpp.h"
#include <string>

/**
 * @file get_missing_placeholder_attribute.hpp
 * @brief Get the missing placeholder attribute.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * @param handle Dataset handle.
 * @param attr_name Name of the attribute containing the missing value placeholder.
 * @param type_class_only Whether to only require identical type classes for the placeholder.
 * By default, we require identity in the types themselves.
 *
 * 
 * @return Handle to the attribute.
 * An error is raised if the attribute is not a scalar or has a different type (or type class, if `type_class_only_ = true`) to the dataset.
 */
inline H5::Attribute get_missing_placeholder_attribute(const H5::DataSet& handle, const char* attr_name, bool type_class_only = false) {
    auto attr = handle.openAttribute(attr_name);
    if (attr.getSpace().getSimpleExtentNdims() != 0) {
        throw std::runtime_error("expected the '" + std::string(attr_name) + "' attribute to be a scalar");
    }

    if (type_class_only) {
        if (attr.getTypeClass() != handle.getTypeClass()) {
            throw std::runtime_error("expected the '" + std::string(attr_name) + "' attribute to have the same type class as its dataset");
        }
    } else {
        if (attr.getDataType() != handle.getDataType()) {
            throw std::runtime_error("expected the '" + std::string(attr_name) + "' attribute to have the same type as its dataset");
        }
    }

    return attr;
}

}

}

#endif
