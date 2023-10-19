#ifndef RITSUKO_HDF5_LOAD_SCALAR_STRING_ATTRIBUTE_HPP
#define RITSUKO_HDF5_LOAD_SCALAR_STRING_ATTRIBUTE_HPP

#include "H5Cpp.h"
#include <string>

/**
 * @file load_scalar_string_attribute.hpp
 * @brief Load a scalar string HDF5 attribute.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * @param attr An Attribute handle.
 * @param field Name of the attribute, for error messages.
 * @param name Name of the object containing the attribute, for error messages.
 *
 * @return The attribute as a string.
 */
inline std::string load_scalar_string_attribute(const H5::Attribute& attr, const char* field, const char* name) {
    if (attr.getTypeClass() != H5T_STRING || attr.getSpace().getSimpleExtentNdims() != 0) {
        throw std::runtime_error(std::string("'") + std::string(field) + "' attribute on '" + std::string(name) + "' should be a scalar string");
    }
    std::string output;
    attr.read(attr.getStrType(), output);
    return output;
}

/**
 * @tparam Object_ HDF5 object class, usually a DataSet or a Group.
 *
 * @param handle Handle to a HDF5 object that can contain attributes.
 * @param field Name of the attribute.
 * @param name Name of the object containing the attribute, for error messages.
 *
 * @return The attribute as a string.
 */
template<class Object_>
std::string load_scalar_string_attribute(const Object_& handle, const char* field, const char* name) {
    if (!handle.attrExists(field)) {
        throw std::runtime_error(std::string("expected a '") + std::string(field) + "' attribute on '" + std::string(name) + "'");
    }
    return load_scalar_string_attribute(handle.openAttribute(field), field, name);
}

}

}

#endif
