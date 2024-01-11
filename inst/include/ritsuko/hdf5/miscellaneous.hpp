#ifndef RITSUKO_MISCELLANEOUS_HPP
#define RITSUKO_MISCELLANEOUS_HPP

#include <string>
#include "H5Cpp.h"

#include "open.hpp"
#include "load_attribute.hpp"
#include "is_utf8_string.hpp"
#include "get_1d_length.hpp"

/**
 * @file miscellaneous.hpp
 * @brief Miscellaneous functions for user convenience.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * @tparam Object_ Type of the HDF5 handle, usually a `DataSet` or `Group`.
 * @param handle HDF5 dataset or group handle.
 * @param name Name of the attribute.
 *
 * @return Attribute handle.
 * An error is raised if `name` does not refer to a scalar attribute.
 */
template<class H5Object_>
inline H5::Attribute open_scalar_attribute(const H5Object_& handle, const char* name) {
    auto attr = open_attribute(handle, name);
    if (!is_scalar(attr)) {
        throw std::runtime_error("expected '" + std::string(name) + "' attribute to be a scalar");
    }
    return attr;
}

/**
 * @tparam Object_ Type of the HDF5 handle, usually a `DataSet` or `Group`.
 * @param handle HDF5 dataset or group handle.
 * @param name Name of the attribute.
 * @tparam utf8 Whether to check for a UTF-8 encoding.
 *
 * @return A string containing the attribute value.
 * This function will fail if the attribute at `name` is not scalar or does not use a string datatype.
 * If `utf8 = true`, it was also fail if the datatype does not use a UTF-8 compatible encoding.
 */
template<class H5Object_>
std::string open_and_load_scalar_string_attribute(const H5Object_& handle, const char* name, bool utf8 = true) {
    auto attr = open_scalar_attribute(handle, name);

    if (utf8) {
        if (!is_utf8_string(attr)) {
            throw std::runtime_error("expected '" + std::string(name) + "' attribute to be a string with a UTF-8 compatible encoding");
        }
    } else {
        if (attr.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("expected '" + std::string(name) + "' attribute to be a string");
        }
    }

    return load_scalar_string_attribute(attr);
}

}

}

#endif
