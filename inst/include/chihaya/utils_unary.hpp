#ifndef CHIHAYA_UTILS_UNARY_HPP
#define CHIHAYA_UTILS_UNARY_HPP

#include "H5Cpp.h"
#include "ritsuko/hdf5/hdf5.hpp"
#include "ritsuko/ritsuko.hpp"

#include <stdexcept>
#include <cstdint>
#include <vector>
#include <string>

#include "utils_misc.hpp"

namespace chihaya {

namespace internal_unary {

inline std::string load_method(const H5::Group& handle) {
    return internal_misc::load_scalar_string_dataset(handle, "method");
}

inline std::string load_side(const H5::Group& handle) {
    return internal_misc::load_scalar_string_dataset(handle, "side");
}

inline void check_along(const H5::Group& handle, const ritsuko::Version& version, const std::vector<size_t>& seed_dimensions, size_t extent) {
    size_t along = internal_misc::load_along(handle, version);

    if (static_cast<size_t>(along) >= seed_dimensions.size()) {
        throw std::runtime_error("'along' should be less than the seed dimensionality");
    }

    if (extent != seed_dimensions[along]) {
        throw std::runtime_error("length of 'value' dataset should be equal to the dimension specified in 'along'");
    }
}

}

}

#endif
