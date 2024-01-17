#ifndef CHIHAYA_UNARY_MATH_HPP
#define CHIHAYA_UNARY_MATH_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include <stdexcept>
#include <vector>
#include <string>

#include "utils_unary.hpp"
#include "utils_misc.hpp"
#include "utils_public.hpp"

/**
 * @file unary_math.hpp
 * @brief Validation for delayed unary math operations.
 */

namespace chihaya {

/**
 * @namespace chihaya::unary_math
 * @brief Namespace for delayed unary math.
 */
namespace unary_math {

/**
 * @param handle An open handle on a HDF5 group representing an unary math operation.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options.
 *
 * @return Details of the object after applying the mathal operation.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, Options& options) {
    auto seed_details = internal_misc::load_seed_details(handle, "seed", version, options);
    if (seed_details.type == STRING) {
        throw std::runtime_error("type of 'seed' should be integer, float or boolean");
    }

    // Checking the method.
    auto method = internal_unary::load_method(handle);
    if (method == "sign") {
        seed_details.type = INTEGER;

    } else if (method == "abs") {
        seed_details.type = std::max(seed_details.type, INTEGER);

    } else if (
        method == "log1p" ||
        method == "sqrt" ||
        method == "exp" ||
        method == "expm1" ||
        method == "ceiling" ||
        method == "floor" || 
        method == "trunc" ||
        method == "sin" ||
        method == "cos" ||
        method == "tan" ||
        method == "acos" ||
        method == "asin" ||
        method == "atan" ||
        method == "sinh" ||
        method == "cosh" ||
        method == "tanh" ||
        method == "acosh" ||
        method == "asinh" ||
        method == "atanh")
    {
        seed_details.type = FLOAT;

    } else if (method == "log") {
        if (!options.details_only) {
            if (handle.exists("base")) {
                if (handle.childObjType("base") != H5O_TYPE_DATASET) {
                    throw std::runtime_error("expected 'base' to be a dataset for a log transformation");
                }
                auto vhandle = handle.openDataSet("base");
                if (!ritsuko::hdf5::is_scalar(vhandle)) {
                    throw std::runtime_error("'base' should be a scalar");
                }

                if (version.lt(1, 1, 0)) {
                    if (vhandle.getTypeClass() != H5T_FLOAT) {
                        throw std::runtime_error("'base' should be a floating-point number");
                    }
                } else {
                    if (ritsuko::hdf5::exceeds_float_limit(vhandle, 64)) {
                        throw std::runtime_error("'base' should have a datatype that fits into a 64-bit float");
                    }
                }
            }
        }
        seed_details.type = FLOAT;

    } else if (method == "round" || method == "signif") {
        if (!options.details_only) {
            auto vhandle = ritsuko::hdf5::open_dataset(handle, "digits");
            if (!ritsuko::hdf5::is_scalar(vhandle)) {
                throw std::runtime_error("'digits' should be a scalar");
            }

            if (version.lt(1, 1, 0)) {
                if (vhandle.getTypeClass() != H5T_INTEGER) {
                    throw std::runtime_error("'digits' should be an integer");
                }
            } else {
                if (ritsuko::hdf5::exceeds_integer_limit(vhandle, 32, true)) {
                    throw std::runtime_error("'digits' should have a datatype that fits into a 32-bit signed integer");
                }
            }
        }
        seed_details.type = FLOAT;

    } else {
        throw std::runtime_error("unrecognized operation in 'method' (got '" + method + "')");
    }

    return seed_details;
}

}

}

#endif
