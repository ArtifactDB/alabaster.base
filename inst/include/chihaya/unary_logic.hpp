#ifndef CHIHAYA_UNARY_LOGIC_HPP
#define CHIHAYA_UNARY_LOGIC_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include <stdexcept>

#include "utils_logic.hpp"
#include "utils_unary.hpp"
#include "utils_type.hpp"
#include "utils_misc.hpp"

/**
 * @file unary_logic.hpp
 *
 * @brief Validation for delayed unary logic operations.
 */

namespace chihaya {

/**
 * @namespace chihaya::unary_logic
 * @brief Namespace for delayed unary logic operations.
 */
namespace unary_logic {

/**
 * @param handle An open handle on a HDF5 group representing an unary logic operation.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options.
 *
 * @return Details of the object after applying the logical operation.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, Options& options) {
    auto seed_details = internal_logic::fetch_seed(handle, "seed", version, options);

    if (!options.details_only) { 
        auto method = internal_unary::load_method(handle);
        if (method != "!" && method != "&&" && method != "||") {
            throw std::runtime_error("unrecognized operation in 'method' (got '" + method + "')");
        }

        // Checking the sidedness.
        if (method != "!") {
            auto side = internal_unary::load_side(handle);
            if (side != "left" && side != "right") {
                throw std::runtime_error("'side' for operation '" + method + "' should be 'left' or 'right' (got '" + side + "')");
            }

            // Checking the value.
            auto vhandle = ritsuko::hdf5::open_dataset(handle, "value");

            try {
                if (version.lt(1, 1, 0)) {
                    if (vhandle.getTypeClass() == H5T_STRING) {
                        throw std::runtime_error("dataset should be integer, float or boolean");
                    }
                } else {
                    auto type = ritsuko::hdf5::open_and_load_scalar_string_attribute(vhandle, "type");
                    auto array_type = internal_type::translate_type_1_1(type);
                    if (array_type != INTEGER && array_type != BOOLEAN && array_type != FLOAT) {
                        throw std::runtime_error("dataset should be integer, float or boolean");
                    }
                    internal_type::check_type_1_1(vhandle, array_type);
                }

                internal_misc::validate_missing_placeholder(vhandle, version);

                size_t ndims = vhandle.getSpace().getSimpleExtentNdims();
                if (ndims == 0) {
                    // scalar operation.
                } else if (ndims == 1) {
                    hsize_t extent;
                    vhandle.getSpace().getSimpleExtentDims(&extent);
                    internal_unary::check_along(handle, version, seed_details.dimensions, extent);
                } else { 
                    throw std::runtime_error("dataset should be scalar or 1-dimensional");
                }
            } catch (std::exception& e) {
                throw std::runtime_error("failed to validate 'value'; " + std::string(e.what()));
            }
        }
    }

    seed_details.type = BOOLEAN;
    return seed_details;
}

}

}

#endif
