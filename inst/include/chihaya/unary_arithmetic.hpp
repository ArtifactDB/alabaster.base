#ifndef CHIHAYA_UNARY_ARITHMETIC_HPP
#define CHIHAYA_UNARY_ARITHMETIC_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include <stdexcept>

#include "utils_public.hpp"
#include "utils_unary.hpp"
#include "utils_type.hpp"
#include "utils_misc.hpp"
#include "utils_arithmetic.hpp"

/**
 * @file unary_arithmetic.hpp
 * @brief Validation for delayed unary arithmetic operations.
 */

namespace chihaya {

/**
 * @namespace chihaya::unary_arithmetic
 * @brief Namespace for delayed unary arithmetic operations.
 */
namespace unary_arithmetic {

/**
 * @param handle An open handle on a HDF5 group representing an unary arithmetic operation.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options.
 *
 * @return Details of the object after applying the arithmetic operation.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, Options& options) {
    auto seed_details = internal_arithmetic::fetch_seed(handle, "seed", version, options);

    auto method = internal_unary::load_method(handle);
    if (!options.details_only) {
        if (!internal_arithmetic::is_valid_operation(method)) {
            throw std::runtime_error("unrecognized operation in 'method' (got '" + method + "')");
        }
    }

    auto side = internal_unary::load_side(handle);
    if (!options.details_only) {
        if (side == "none") {
            if (method != "+" && method != "-") {
                throw std::runtime_error("'side' cannot be 'none' for operation '" + method + "'");
            } 
        } else if (side != "left" && side != "right") {
            throw std::runtime_error("'side' for operation '" + method + "' should be 'left' or 'right' (got '" + side + "')");
        }
    }

    // If side = none, we set it to INTEGER to promote BOOLEANs to integer (implicit multiplication by +/-1).
    ArrayType min_type = INTEGER;

    if (side != "none") {
        auto vhandle = ritsuko::hdf5::open_dataset(handle, "value");
        
        try {
            if (version.lt(1, 1, 0)) {
                if (vhandle.getTypeClass() == H5T_STRING) {
                    throw std::runtime_error("dataset should be integer, float or boolean");
                } else if (vhandle.getTypeClass() == H5T_FLOAT) {
                    min_type = FLOAT;
                }
            } else {
                auto type = ritsuko::hdf5::open_and_load_scalar_string_attribute(vhandle, "type");
                min_type = internal_type::translate_type_1_1(type);
                if (min_type != INTEGER && min_type != BOOLEAN && min_type != FLOAT) {
                    throw std::runtime_error("dataset should be integer, float or boolean");
                }
                internal_type::check_type_1_1(vhandle, min_type);
            }

            if (!options.details_only) {
                internal_misc::validate_missing_placeholder(vhandle, version);
        
                auto vspace = vhandle.getSpace();
                size_t ndims = vspace.getSimpleExtentNdims();
                if (ndims == 0) {
                    // scalar operation.
                } else if (ndims == 1) {
                    hsize_t extent;
                    vspace.getSimpleExtentDims(&extent);
                    internal_unary::check_along(handle, version, seed_details.dimensions, extent);
                } else { 
                    throw std::runtime_error("dataset should be scalar or 1-dimensional");
                }
            }

        } catch (std::exception& e) {
            throw std::runtime_error("failed to validate 'value'; " + std::string(e.what()));
        }
    }

    // Determining type promotion rules.
    seed_details.type = internal_arithmetic::determine_output_type(min_type, seed_details.type, method);

    return seed_details;
}

}

}

#endif
