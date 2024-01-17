#ifndef CHIHAYA_SUBSET_ASSIGNMENT_HPP
#define CHIHAYA_SUBSET_ASSIGNMENT_HPP

#include "H5Cpp.h"
#include "ritsuko/hdf5/hdf5.hpp"
#include "ritsuko/ritsuko.hpp"

#include <stdexcept>
#include <vector>

#include "utils_public.hpp"
#include "utils_list.hpp"
#include "utils_misc.hpp"
#include "utils_subset.hpp"

/**
 * @file subset_assignment.hpp
 * @brief Validation for delayed subset assignment.
 */

namespace chihaya {

/**
 * @namespace chihaya::subset_assignment
 * @brief Namespace for delayed subset assignment.
 */
namespace subset_assignment {

/**
 * @param handle An open handle on a HDF5 group representing a subset assignment.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options.
 *
 * @return Details of the object after subset assignment.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, Options& options) {
    auto seed_details = internal_misc::load_seed_details(handle, "seed", version, options);
    const auto& seed_dims = seed_details.dimensions;

    auto value_details = internal_misc::load_seed_details(handle, "value", version, options);
    if (!options.details_only) {
        if ((value_details.type == STRING) != (seed_details.type == STRING)) {
            throw std::runtime_error("both or neither of the 'seed' and 'value' arrays should contain strings");
        }
        if (seed_dims.size() != value_details.dimensions.size()) {
            throw std::runtime_error("'seed' and 'value' arrays should have the same dimensionalities");
        }

        auto ihandle = ritsuko::hdf5::open_group(handle, "index");
        auto collected = internal_subset::validate_index_list(ihandle, seed_dims, version);
        auto expected_dims = seed_dims;
        for (auto p : collected) {
            expected_dims[p.first] = p.second;
        }

        if (!internal_misc::are_dimensions_equal(expected_dims, value_details.dimensions)) {
            throw std::runtime_error("'value' dimension extents are not consistent with lengths of indices in 'index'");
        }
    }

    // Promotion.
    seed_details.type = std::max(seed_details.type, value_details.type);
    return seed_details;
}

}

}

#endif
