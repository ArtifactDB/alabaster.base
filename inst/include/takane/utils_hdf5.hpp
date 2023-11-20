#ifndef TAKANE_UTILS_HDF5_HPP
#define TAKANE_UTILS_HDF5_HPP

#include <unordered_set>
#include <string>
#include <cstdint>
#include <vector>
#include <stdexcept>

#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

namespace takane {

namespace internal_hdf5 {

inline void validate_string_format(const H5::DataSet& handle, hsize_t len, const std::string& format, bool has_missing, const std::string& missing_value, hsize_t buffer_size) {
    if (format == "date") {
        ritsuko::hdf5::load_1d_string_dataset(
            handle, 
            len,
            buffer_size,
            [&](size_t, const char* p, size_t l) {
                std::string x(p, p + l);
                if (has_missing && missing_value == x) {
                    return;
                }
                if (!ritsuko::is_date(p, l)) {
                    throw std::runtime_error("expected a date-formatted string (got '" + x + "')");
                }
            }
        );

    } else if (format == "date-time") {
        ritsuko::hdf5::load_1d_string_dataset(
            handle, 
            len,
            buffer_size,
            [&](size_t, const char* p, size_t l) {
                std::string x(p, p + l);
                if (has_missing && missing_value == x) {
                    return;
                }
                if (!ritsuko::is_rfc3339(p, l)) {
                    throw std::runtime_error("expected a date/time-formatted string (got '" + x + "')");
                }
            }
        );

    } else if (format != "none") {
        throw std::runtime_error("unsupported format '" + format + "'");
    }
}

inline hsize_t validate_factor_levels(const H5::Group& handle, const std::string& name, hsize_t buffer_size) {
    auto lhandle = ritsuko::hdf5::get_dataset(handle, name.c_str());
    if (lhandle.getTypeClass() != H5T_STRING) {
        throw std::runtime_error("expected a string datatype for '" + name + "'");
    }

    auto len = ritsuko::hdf5::get_1d_length(lhandle.getSpace(), false);
    std::unordered_set<std::string> present;

    ritsuko::hdf5::load_1d_string_dataset(
        lhandle,
        len,
        buffer_size,
        [&](hsize_t, const char* p, size_t len) {
            std::string x(p, p + len);
            if (present.find(x) != present.end()) {
                throw std::runtime_error("'" + name + "' contains duplicated factor level '" + x + "'");
            }
            present.insert(std::move(x));
        }
    );

    return len;
}

inline hsize_t validate_factor_codes(const H5::Group& handle, const std::string& name, hsize_t num_levels, hsize_t buffer_size, bool allow_missing = true) {
    auto chandle = ritsuko::hdf5::get_dataset(handle, name.c_str());
    if (ritsuko::hdf5::exceeds_integer_limit(chandle, 32, true)) {
        throw std::runtime_error("expected a datatype for '" + name + "' that fits in a 32-bit signed integer");
    }

    auto len = ritsuko::hdf5::get_1d_length(chandle.getSpace(), false);
    auto block_size = ritsuko::hdf5::pick_1d_block_size(chandle.getCreatePlist(), len, buffer_size);
    std::vector<int32_t> buffer(block_size);

    bool has_missing = false;
    int32_t missing_placeholder = 0;
    if (allow_missing) {
        const char* missing_attr_name = "missing-value-placeholder";
        has_missing = chandle.attrExists(missing_attr_name);
        if (has_missing) {
            auto missing_attr = ritsuko::hdf5::get_missing_placeholder_attribute(chandle, missing_attr_name);
            missing_attr.read(H5::PredType::NATIVE_INT32, &missing_placeholder);
        }
    }

    ritsuko::hdf5::iterate_1d_blocks(
        len,
        block_size,
        [&](hsize_t, hsize_t len, const H5::DataSpace& memspace, const H5::DataSpace& dataspace) {
            chandle.read(buffer.data(), H5::PredType::NATIVE_INT32, memspace, dataspace);
            for (hsize_t i = 0; i < len; ++i) {
                if (has_missing && buffer[i] == missing_placeholder) {
                    continue;
                }
                if (buffer[i] < 0) {
                    throw std::runtime_error("expected factor codes to be non-negative");
                }
                if (static_cast<hsize_t>(buffer[i]) >= num_levels) {
                    throw std::runtime_error("expected factor codes to be less than the number of levels");
                }
            }
        }
    );

    return len;
}

}

}

#endif
