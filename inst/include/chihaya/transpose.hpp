#ifndef CHIHAYA_TRANSPOSE_HPP
#define CHIHAYA_TRANSPOSE_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstdint>

#include "utils_misc.hpp"

/**
 * @file transpose.hpp
 * @brief Validation for delayed transposition.
 */

namespace chihaya {

/**
 * @namespace chihaya::transpose
 * @brief Namespace for delayed transposition.
 */
namespace transpose {

/**
 * @cond
 */
namespace internal {

template<typename Perm_>
std::vector<size_t> check_permutation(const H5::DataSet& phandle, size_t ndims, const H5::PredType& h5type, const std::vector<size_t>& input_dimensions, bool details_only) {
    if (ndims != input_dimensions.size()) {
        throw std::runtime_error("length of 'permutation' should match dimensionality of 'seed'");
    }

    std::vector<Perm_> permutation(ndims);
    phandle.read(permutation.data(), h5type);

    std::vector<size_t> new_dimensions(ndims);
    for (size_t p = 0; p < ndims; ++p) {
        auto current = permutation[p];
        if (current < 0) {
            throw std::runtime_error("'permutation' should contain non-negative indices");
        }
        if (static_cast<size_t>(current) >= ndims) {
            throw std::runtime_error("'permutation' contains out-of-bounds indices");
        }
        new_dimensions[p] = input_dimensions[permutation[p]];
    }

    if (!details_only) {
        std::sort(permutation.begin(), permutation.end());
        for (size_t p = 0; p < permutation.size(); ++p) {
            if (p != static_cast<size_t>(permutation[p])) {
                throw std::runtime_error("indices in 'permutation' should be unique for a transpose operation");
            }
        }
    }

    return new_dimensions;
}

}
/**
 * @endcond
 */

/**
 * @param handle An open handle on a HDF5 group representing a transposition.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options.
 *
 * @return Details of the transposed object.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, Options& options) {
    auto seed_details = internal_misc::load_seed_details(handle, "seed", version, options);

    auto phandle = ritsuko::hdf5::open_dataset(handle, "permutation");
    auto ndims = ritsuko::hdf5::get_1d_length(phandle, false);

    if (version.lt(1, 1, 0)) {
        if (phandle.getTypeClass() != H5T_INTEGER) {
            throw std::runtime_error("'permutation' should be integer");
        }
        seed_details.dimensions = internal::check_permutation<int>(phandle, ndims, H5::PredType::NATIVE_INT, seed_details.dimensions, options.details_only);
    } else {
        if (ritsuko::hdf5::exceeds_integer_limit(phandle, 64, false)) {
            throw std::runtime_error("'permutation' should have a datatype that can be represented by a 64-bit unsigned integer");
        }
        seed_details.dimensions = internal::check_permutation<uint64_t>(phandle, ndims, H5::PredType::NATIVE_UINT64, seed_details.dimensions, options.details_only);
    }

    return seed_details;
}

}

}

#endif
