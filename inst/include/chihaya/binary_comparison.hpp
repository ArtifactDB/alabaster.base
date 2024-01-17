#ifndef CHIHAYA_BINARY_COMPARISON_HPP
#define CHIHAYA_BINARY_COMPARISON_HPP

#include "H5Cpp.h"
#include "ritsuko/hdf5/hdf5.hpp"
#include "ritsuko/ritsuko.hpp"

#include <stdexcept>
#include <vector>

#include "utils_public.hpp"
#include "utils_misc.hpp"
#include "utils_comparison.hpp"
#include "utils_unary.hpp"

/**
 * @file binary_comparison.hpp
 * @brief Validation for delayed binary comparisons.
 */

namespace chihaya {

/**
 * @namespace chihaya::binary_comparison
 * @brief Namespace for delayed binary comparisons.
 */
namespace binary_comparison {

/**
 * @param handle An open handle on a HDF5 group representing a binary comparison.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options.
 *
 * @return Details of the object after applying the comparison operation.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, Options& options) {
    auto left_details = internal_misc::load_seed_details(handle, "left", version, options);
    auto right_details = internal_misc::load_seed_details(handle, "right", version, options);

    if (!options.details_only) {
        if (!internal_misc::are_dimensions_equal(left_details.dimensions, right_details.dimensions)) {
            throw std::runtime_error("'left' and 'right' should have the same dimensions");
        }

        if ((left_details.type == STRING) != (right_details.type == STRING)) {
            throw std::runtime_error("both or neither of 'left' and 'right' should contain strings");
        }
    }

    auto method = internal_unary::load_method(handle);
    if (!options.details_only) {
        if (!internal_comparison::is_valid_operation(method)) {
            throw std::runtime_error("unrecognized 'method' (" + method + ")");
        }
    }

    left_details.type = BOOLEAN;
    return left_details;
}

}

}

#endif
