#ifndef CHIHAYA_SUBSET_HPP
#define CHIHAYA_SUBSET_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include <stdexcept>
#include <vector>
#include <cstdint>

#include "utils_public.hpp"
#include "utils_list.hpp"
#include "utils_misc.hpp"
#include "utils_subset.hpp"

/**
 * @file subset.hpp
 * @brief Validation for delayed subsets.
 */

namespace chihaya {

/**
 * @namespace chihaya::subset
 * @brief Namespace for delayed subsets.
 */
namespace subset {

/**
 * @param handle An open handle on a HDF5 group representing a subset operation.
 * @param version Verison of the **chihaya** specification.
 * @param options Validation options.
 *
 * @return Details of the subsetted object.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, Options& options) {
    auto seed_details = internal_misc::load_seed_details(handle, "seed", version, options);
    auto& seed_dims = seed_details.dimensions;

    auto ihandle = ritsuko::hdf5::open_group(handle, "index");
    auto collected = internal_subset::validate_index_list(ihandle, seed_dims, version);
    for (auto p : collected) {
        seed_dims[p.first] = p.second;
    }

    return seed_details;
}

}

}

#endif
