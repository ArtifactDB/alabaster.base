#ifndef CHIHAYA_DIMNAMES_HPP
#define CHIHAYA_DIMNAMES_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"
#include <stdexcept>
#include "utils_list.hpp"
#include "utils_public.hpp"

/**
 * @file dimnames.hpp
 * @brief Validation for delayed dimnames assignment.
 */

namespace chihaya {

namespace dimnames {

/**
 * @param handle An open handle on a HDF5 group representing a dimnames assignment operation.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options.
 *
 * @return Details of the object after assigning dimnames.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, Options& options) {
    ArrayDetails seed_details = internal_misc::load_seed_details(handle, "seed", version, options);
    if (!handle.exists("dimnames")) {
        throw std::runtime_error("expected a 'dimnames' group");
    }

    if (!options.details_only) {
        internal_dimnames::validate(handle, seed_details.dimensions, version);
    }

    return seed_details;
}

}

}

#endif
