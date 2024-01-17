#ifndef CHIHAYA_EXTERNAL_HDF5_HPP
#define CHIHAYA_EXTERNAL_HDF5_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include "minimal_array.hpp"

/**
 * @file external_hdf5.hpp
 * @brief Validation for external HDF5 arrays.
 */

namespace chihaya {

/**
 * @namespace chihaya::external_hdf5
 * @brief Namespace for external HDF5 arrays.
 */
namespace external_hdf5 {

/**
 * @param handle An open handle on a HDF5 group representing an external HDF5 array.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options.
 *
 * @return Details of the external HDF5 array.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, Options& options) {
    auto deets = minimal_array::validate(handle, version, options);

    if (!options.details_only) {
        auto fhandle = ritsuko::hdf5::open_dataset(handle, "file");
        if (!ritsuko::hdf5::is_scalar(fhandle)) {
            throw std::runtime_error("'file' should be a scalar");
        }
        if (!ritsuko::hdf5::is_utf8_string(fhandle)) {
            throw std::runtime_error("'file' should have a datatype that can be represented by a UTF-8 encoded string");
        }

        auto nhandle = ritsuko::hdf5::open_dataset(handle, "name");
        if (!ritsuko::hdf5::is_scalar(nhandle)) {
            throw std::runtime_error("'name' should be a scalar");
        }
        if (!ritsuko::hdf5::is_utf8_string(nhandle)) {
            throw std::runtime_error("'name' should have a datatype that can be represented by a UTF-8 encoded string");
        }
    }

    return deets;
}

}

}

#endif
