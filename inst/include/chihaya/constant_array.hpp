#ifndef CHIHAYA_CONSTANT_ARRAY_HPP
#define CHIHAYA_CONSTANT_ARRAY_HPP

/**
 * @file constant_array.hpp
 * @brief Constant array, stored inside the file.
 */

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include <vector>
#include <cstdint>
#include <stdexcept>

namespace chihaya {

/**
 * @namespace chihaya::constant_array
 * @brief Namespace for constant arrays.
 */
namespace constant_array {

/**
 * @param handle An open handle on a HDF5 group representing a dense array.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options.
 *
 * @return Details of the constant array.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, [[maybe_unused]] Options& options) {
    ArrayDetails output;

    {
        auto dhandle = ritsuko::hdf5::open_dataset(handle, "dimensions");
        size_t size = ritsuko::hdf5::get_1d_length(dhandle, false);
        if (size == 0) {
            throw std::runtime_error("'dimensions' should have non-zero length");
        }

        if (version.lt(1, 1, 0)) {
            if (dhandle.getTypeClass() != H5T_INTEGER) {
                throw std::runtime_error("'dimensions' should be integer");
            }
            std::vector<int> dims_tmp(size);
            dhandle.read(dims_tmp.data(), H5::PredType::NATIVE_INT);
            for (auto d : dims_tmp) {
                if (d < 0) {
                    throw std::runtime_error("'dimensions' should contain non-negative values");
                }
            }
            output.dimensions.insert(output.dimensions.end(), dims_tmp.begin(), dims_tmp.end());

        } else {
            if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 64, false)) {
                throw std::runtime_error("datatype of 'dimensions' should fit inside a 64-bit unsigned integer");
            }
            std::vector<uint64_t> dims(size);
            dhandle.read(dims.data(), H5::PredType::NATIVE_UINT64);
            output.dimensions.insert(output.dimensions.end(), dims.begin(), dims.end());
        }
    }
 
    {
        auto vhandle = ritsuko::hdf5::open_dataset(handle, "value");
        if (!ritsuko::hdf5::is_scalar(vhandle)) {
            throw std::runtime_error("'value' should be a scalar");
        }

        try {
            if (version.lt(1, 1, 0)) {
                output.type = internal_type::translate_type_0_0(vhandle.getTypeClass());
            } else {
                auto type = ritsuko::hdf5::open_and_load_scalar_string_attribute(vhandle, "type");
                output.type = internal_type::translate_type_1_1(type);
                internal_type::check_type_1_1(vhandle, output.type);
            }

            if (!options.details_only) {
                internal_misc::validate_missing_placeholder(vhandle, version);
            }

            if (vhandle.getTypeClass() == H5T_STRING) {
                ritsuko::hdf5::validate_scalar_string_dataset(vhandle);
            }

        } catch (std::exception& e) {
            throw std::runtime_error("failed to validate 'value'; " + std::string(e.what()));
        }
    }

    return output;
}

}

}

#endif
