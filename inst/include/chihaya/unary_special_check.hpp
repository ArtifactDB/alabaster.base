#ifndef CHIHAYA_UNARY_SPECIAL_CHECK_HPP
#define CHIHAYA_UNARY_SPECIAL_CHECK_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"

#include <stdexcept>

#include "utils_unary.hpp"

/**
 * @file unary_special_check.hpp
 * @brief Validation for delayed unary special checks.
 */

namespace chihaya {

/**
 * @namespace chihaya::unary_special_check
 * @brief Namespace for delayed unary special checks.
 */
namespace unary_special_check {

/**
 * @param handle An open handle on a HDF5 group representing an unary special check operation.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options.
 *
 * @return Details of the object after applying the special check.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, Options& options) {
    auto seed_details = internal_misc::load_seed_details(handle, "seed", version, options);
    if (seed_details.type == STRING) {
        throw std::runtime_error("'seed' should contain integer, float or boolean values");
    }

    // Checking the method.
    auto method = internal_unary::load_method(handle);
    if (!options.details_only) {
        if (method != "is_nan" &&
            method != "is_finite" &&
            method != "is_infinite")
        {
            throw std::runtime_error("unrecognized 'method' (" + method + ")");
        }
    }

    seed_details.type = BOOLEAN;
    return seed_details;
}

}

}

#endif
