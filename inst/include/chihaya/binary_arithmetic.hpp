#ifndef CHIHAYA_BINARY_ARITHMETIC_HPP
#define CHIHAYA_BINARY_ARITHMETIC_HPP

#include "H5Cpp.h"
#include "ritsuko/hdf5/hdf5.hpp"
#include "ritsuko/ritsuko.hpp"

#include <stdexcept>
#include <vector>
#include <string>

#include "utils_public.hpp"
#include "utils_arithmetic.hpp"
#include "utils_misc.hpp"
#include "utils_unary.hpp"

/**
 * @file binary_arithmetic.hpp
 * @brief Validation for delayed binary arithmetic operations.
 */

namespace chihaya {

/**
 * @namespace chihaya::binary_arithmetic
 * @brief Namespace for delayed binary arithmetic operations.
 */
namespace binary_arithmetic {

/**
 * @param handle An open handle on a HDF5 group representing a binary arithmetic operation.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options.
 *
 * @return Details of the object after applying the arithmetic operation.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, Options& options) {
    auto left_details = internal_arithmetic::fetch_seed(handle, "left", version, options);
    auto right_details = internal_arithmetic::fetch_seed(handle, "right", version, options);

    if (!options.details_only) {
        if (!internal_misc::are_dimensions_equal(left_details.dimensions, right_details.dimensions)) {
            throw std::runtime_error("'left' and 'right' should have the same dimensions");
        }
    }

    auto method = internal_unary::load_method(handle);
    if (!options.details_only) {
        if (!internal_arithmetic::is_valid_operation(method)) {
            throw std::runtime_error("unrecognized 'method' (" + method + ")");
        }
    }

    left_details.type = internal_arithmetic::determine_output_type(left_details.type, right_details.type, method);
    return left_details;
}

}

}

#endif
