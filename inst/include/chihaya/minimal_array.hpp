#ifndef CHIHAYA_MINIMAL_ARRAY_HPP
#define CHIHAYA_MINIMAL_ARRAY_HPP

#include "H5Cpp.h"
#include "ritsuko/hdf5/hdf5.hpp"
#include "ritsuko/ritsuko.hpp"

#include <vector>
#include <stdexcept>
#include <cstdint>
#include <algorithm>

#include "utils_misc.hpp"

namespace chihaya {

namespace minimal_array {

inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, [[maybe_unused]] Options& options) {
    auto dhandle = ritsuko::hdf5::open_dataset(handle, "dimensions");
    auto len = ritsuko::hdf5::get_1d_length(dhandle, false);
    std::vector<uint64_t> dimensions(len);

    if (version.lt(1, 1, 0)) {
        if (dhandle.getTypeClass() != H5T_INTEGER) {
            throw std::runtime_error("'dimensions' should be integer");
        }
        std::vector<int64_t> dimensions_tmp(len);
        dhandle.read(dimensions_tmp.data(), H5::PredType::NATIVE_INT64);
        for (auto d : dimensions_tmp) {
            if (d < 0) {
                throw std::runtime_error("elements in 'dimensions' should be non-negative");
            }
        } 
        std::copy(dimensions_tmp.begin(), dimensions_tmp.end(), dimensions.begin());
    } else {
        if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 64, false)) {
            throw std::runtime_error("datatype of 'dimensions' should fit in a 64-bit unsigned integer");
        }
        dhandle.read(dimensions.data(), H5::PredType::NATIVE_UINT64);
    }

    ArrayType atype;
    {
        auto type = internal_misc::load_scalar_string_dataset(handle, "type");
        if (type == "BOOLEAN") {
            atype = BOOLEAN;
        } else if (type == "INTEGER") {
            atype = INTEGER;
        } else if (type == "FLOAT") {
            atype = FLOAT;
        } else if (type == "STRING") {
            atype = STRING;
        } else {
            throw std::runtime_error("unknown 'type' (" + type + ")");
        }
    }

    return ArrayDetails(atype, std::vector<size_t>(dimensions.begin(), dimensions.end()));
}

}

}

#endif
