#ifndef RITSUKO_HDF5_GET_ATTRIBUTE_HPP
#define RITSUKO_HDF5_GET_ATTRIBUTE_HPP

#include "H5Cpp.h"
#include <string>

/**
 * @file get_scalar_attribute.hpp
 * @brief Helper to get a scalar attribute handle.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Convenient wrapper to get a scalar attribute with all of the usual error checks.
 *
 * @tparam Object_ Type of the HDF5 handle, usually a `DataSet` or `Group`.
 * @param handle HDF5 dataset or group handle.
 * @param name Name of the attribute.
 *
 * @return Attribute handle.
 */
template<class Object_>
H5::Attribute get_scalar_attribute(const Object_& handle, const char* name) {
    if (!handle.attrExists(name)) {
        throw std::runtime_error("expected an attribute at '" + std::string(name) + "'");
    }

    auto attr = handle.openAttribute(name);
    if (attr.getSpace().getSimpleExtentNdims() != 0) {
        throw std::runtime_error("expected a scalar attribute at '" + std::string(name) + "'");
    }

    return attr;
}

}

}

#endif
