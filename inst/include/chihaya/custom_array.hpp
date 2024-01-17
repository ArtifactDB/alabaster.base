#ifndef CHIHAYA_CUSTOM_ARRAY_HPP
#define CHIHAYA_CUSTOM_ARRAY_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include <vector>
#include "minimal_array.hpp"

/**
 * @file custom_array.hpp
 *
 * @brief Validation for custom third-party arrays.
 */

namespace chihaya {

/**
 * @namespace chihaya::custom_array
 * @brief Namespace for custom third-party arrays.
 */
namespace custom_array {

/**
 * @param handle An open handle on a HDF5 group representing an external array.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options.
 *
 * @return Details of the custom array.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, Options& options) {
    return minimal_array::validate(handle, version, options);
}

}

}

#endif
