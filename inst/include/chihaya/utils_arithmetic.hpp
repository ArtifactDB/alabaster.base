#ifndef CHIHAYA_UTILS_ARITHMETIC_HPP
#define CHIHAYA_UTILS_ARITHMETIC_HPP

#include "H5Cpp.h"
#include "ritsuko/hdf5/hdf5.hpp"
#include "ritsuko/ritsuko.hpp"

#include <stdexcept>
#include <string>

#include "utils_public.hpp"
#include "utils_misc.hpp"

namespace chihaya {

namespace internal_arithmetic {

inline ArrayDetails fetch_seed(const H5::Group& handle, const std::string& target, const ritsuko::Version& version, Options& options) {
    auto output = internal_misc::load_seed_details(handle, target, version, options);
    if (output.type == STRING) {
        throw std::runtime_error("type of '" + target + "' should be integer, float or boolean");
    }
    return output;
}

inline bool is_valid_operation(const std::string& method) {
    return (method == "+" ||
        method == "-" ||
        method == "/" ||
        method == "*" || 
        method == "%/%" ||
        method == "^" ||
        method == "%%");
}

inline ArrayType determine_output_type(const ArrayType& first, const ArrayType& second, const std::string& method) {
    if (method == "/") {
        return FLOAT;
    } else if (method == "%/%") {
        return INTEGER;
    }

    auto output = std::max(first, second);
    if (output == BOOLEAN) {
        return INTEGER;
    }

    return output;
}

}

}

#endif
