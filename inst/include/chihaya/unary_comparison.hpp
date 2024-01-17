#ifndef CHIHAYA_UNARY_COMPARISON_HPP
#define CHIHAYA_UNARY_COMPARISON_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include <stdexcept>

#include "utils_public.hpp"
#include "utils_comparison.hpp"
#include "utils_unary.hpp"
#include "utils_misc.hpp"
#include "utils_type.hpp"

/**
 * @file unary_comparison.hpp
 * @brief Validation for delayed unary comparisons.
 */

namespace chihaya {

/**
 * @namespace chihaya::unary_comparison
 * @brief Namespace for delayed unary comparisons.
 */
namespace unary_comparison {

/**
 * @param handle An open handle on a HDF5 group representing an unary comparison operation.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options.
 *
 * @return Details of the object after applying the comparison operation.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, Options& options) {
    auto seed_details = internal_misc::load_seed_details(handle, "seed", version, options);

    if (!options.details_only) {
        auto method = internal_unary::load_method(handle);
        if (!internal_comparison::is_valid_operation(method)) {
            throw std::runtime_error("unrecognized operation in 'method' (got '" + method + "')");
        }

        auto side = internal_unary::load_side(handle);
        if (side != "left" && side != "right") {
            throw std::runtime_error("'side' should be either 'left' or 'right' (got '" + side + "')");
        }

        // Checking the value.
        auto vhandle = ritsuko::hdf5::open_dataset(handle, "value");
        try {
            if (version.lt(1, 1, 0)) {
                if ((seed_details.type == STRING) != (vhandle.getTypeClass() == H5T_STRING)) {
                    throw std::runtime_error("both or neither of 'seed' and 'value' should contain strings");
                }
            } else {
                auto type = ritsuko::hdf5::open_and_load_scalar_string_attribute(vhandle, "type");
                auto tt = internal_type::translate_type_1_1(type);
                if ((tt == STRING) != (seed_details.type == STRING)) {
                    throw std::runtime_error("both or neither of 'seed' and 'value' should contain strings");
                }
                internal_type::check_type_1_1(vhandle, tt);
            }

            internal_misc::validate_missing_placeholder(vhandle, version);

            size_t ndims = vhandle.getSpace().getSimpleExtentNdims();
            if (ndims == 0) { // scalar operation.
                if (vhandle.getTypeClass() == H5T_STRING) {
                    ritsuko::hdf5::validate_scalar_string_dataset(vhandle);
                }

            } else if (ndims == 1) {
                hsize_t extent;
                vhandle.getSpace().getSimpleExtentDims(&extent);
                internal_unary::check_along(handle, version, seed_details.dimensions, extent);
                if (vhandle.getTypeClass() == H5T_STRING) {
                    ritsuko::hdf5::validate_1d_string_dataset(vhandle, extent, 1000000);
                }

            } else { 
                throw std::runtime_error("dataset should be scalar or 1-dimensional");
            }

        } catch (std::exception& e) {
            throw std::runtime_error("failed to validate 'value'; " + std::string(e.what()));
        }
    }

    seed_details.type = BOOLEAN;
    return seed_details;
}

}

}

#endif
