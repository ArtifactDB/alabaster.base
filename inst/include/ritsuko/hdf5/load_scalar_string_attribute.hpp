#ifndef RITSUKO_HDF5_LOAD_SCALAR_STRING_ATTRIBUTE_HPP
#define RITSUKO_HDF5_LOAD_SCALAR_STRING_ATTRIBUTE_HPP

#include "H5Cpp.h"
#include <string>

#include "get_name.hpp"

/**
 * @file load_scalar_string_attribute.hpp
 * @brief Load a scalar string HDF5 attribute.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * @param attr An ``Attribute`` handle.
 * @return The attribute as a string.
 */
inline std::string load_scalar_string_attribute(const H5::Attribute& attr) {
    if (attr.getTypeClass() != H5T_STRING || attr.getSpace().getSimpleExtentNdims() != 0) {
        std::string name;
        attr.getName(name);
        throw std::runtime_error("expected attribute '" + name + "' to be a scalar string");
    }
    std::string output;
    attr.read(attr.getStrType(), output);
    return output;
}

/**
 * @tparam Object_ HDF5 object class, usually a ``DataSet`` or a ``Group``.
 *
 * @param handle Handle to a HDF5 object that can contain attributes.
 * @param field Name of the attribute.
 *
 * @return The attribute as a string.
 */
template<class Object_>
std::string load_scalar_string_attribute(const Object_& handle, const char* field) {
    if (!handle.attrExists(field)) {
        throw std::runtime_error("expected a '" + std::string(field) + "' attribute to be present");
    }
    return load_scalar_string_attribute(handle.openAttribute(field));
}

}

}

#endif
