#ifndef UZUKI2_VALIDATE_HPP
#define UZUKI2_VALIDATE_HPP

#include "Dummy.hpp"
#include "parse.hpp"

#include <vector>
#include <algorithm>
#include <stdexcept>

/**
 * @file validate.hpp
 *
 * @brief Validate HDF5 file contents against the **uzuki2** spec.
 */

namespace uzuki2 {

/**
 * Validate HDF5 file contents against the **uzuki2** specification.
 * Any invalid representations will cause an error to be thrown.
 *
 * @param handle Handle for a HDF5 group corresponding to the list.
 * @param name Name of the HDF5 group corresponding to `handle`. 
 * Only used for error messages.
 * @param num_external Expected number of external references. 
 */
inline void validate(const H5::Group& handle, const std::string& name, int num_external = 0) {
    DummyExternals ext(num_external);
    parse<DummyProvisioner>(handle, name, ext);
    return;
}

/**
 * Validate HDF5 file contents against the **uzuki2** specification.
 * Any invalid representations will cause an error to be thrown.
 *
 * @param file Path to a HDF5 file.
 * @param name Name of the HDF5 group containing the list in `file`.
 * @param num_external Expected number of external references. 
 */
inline void validate(const std::string& file, const std::string& name, int num_external = 0) {
    DummyExternals ext(num_external);
    parse<DummyProvisioner>(file, name, ext);
    return;
}

}

#endif
