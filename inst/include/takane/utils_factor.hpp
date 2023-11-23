#ifndef TAKANE_UTILS_FACTOR_HPP
#define TAKANE_UTILS_FACTOR_HPP

#include <unordered_set>
#include <string>
#include <cstdint>
#include <vector>
#include <stdexcept>

#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

namespace takane {

namespace internal_factor {

template<class H5Object_>
void check_ordered_attribute(const H5Object_& handle) {
    if (!handle.attrExists("ordered")) {
        return;
    }

    auto attr = handle.openAttribute("ordered");
    if (!ritsuko::hdf5::is_scalar(attr)) {
        throw std::runtime_error("expected 'ordered' attribute to be a scalar");
    }
    if (ritsuko::hdf5::exceeds_integer_limit(attr, 32, true)) {
        throw std::runtime_error("expected 'ordered' attribute to have a datatype that fits in a 32-bit signed integer");
    }
}

inline hsize_t validate_factor_levels(const H5::Group& handle, const std::string& name, hsize_t buffer_size) {
    auto lhandle = ritsuko::hdf5::open_dataset(handle, name.c_str());
    if (lhandle.getTypeClass() != H5T_STRING) {
        throw std::runtime_error("expected a string datatype for '" + name + "'");
    }

    auto len = ritsuko::hdf5::get_1d_length(lhandle.getSpace(), false);
    std::unordered_set<std::string> present;

    ritsuko::hdf5::Stream1dStringDataset stream(&lhandle, len, buffer_size);
    for (hsize_t i = 0; i < len; ++i, stream.next()) {
        auto x = stream.steal();
        if (present.find(x) != present.end()) {
            throw std::runtime_error("'" + name + "' contains duplicated factor level '" + x + "'");
        }
        present.insert(std::move(x));
    }

    return len;
}

inline hsize_t validate_factor_codes(const H5::Group& handle, const std::string& name, hsize_t num_levels, hsize_t buffer_size, bool allow_missing = true) {
    auto chandle = ritsuko::hdf5::open_dataset(handle, name.c_str());
    if (ritsuko::hdf5::exceeds_integer_limit(chandle, 64, false)) {
        throw std::runtime_error("expected a datatype for '" + name + "' that fits in a 64-bit unsigned integer");
    }

    bool has_missing = false;
    int32_t missing_placeholder = 0;
    if (allow_missing) {
        auto missingness = ritsuko::hdf5::open_and_load_optional_numeric_missing_placeholder<int32_t>(chandle, "missing-value-placeholder");
        has_missing = missingness.first;
        missing_placeholder = missingness.second;
    }

    auto len = ritsuko::hdf5::get_1d_length(chandle.getSpace(), false);
    ritsuko::hdf5::Stream1dNumericDataset<int32_t> stream(&chandle, len, buffer_size);
    for (hsize_t i = 0; i < len; ++i, stream.next()) {
        auto x = stream.get();
        if (has_missing && x == missing_placeholder) {
            continue;
        }
        if (static_cast<hsize_t>(x) >= num_levels) {
            throw std::runtime_error("expected factor codes to be less than the number of levels");
        }
    }

    return len;
}

}

}

#endif
