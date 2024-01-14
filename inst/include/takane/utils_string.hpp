#ifndef TAKANE_UTILS_STRING_HPP
#define TAKANE_UTILS_STRING_HPP

#include <unordered_set>
#include <string>
#include <cstdint>
#include <vector>
#include <stdexcept>

#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

namespace takane {

namespace internal_string {

template<class H5Object_>
std::string fetch_format_attribute(const H5Object_& handle) {
    if (!handle.attrExists("format")) {
        return "none";
    }

    auto attr = handle.openAttribute("format");
    if (!ritsuko::hdf5::is_scalar(attr)) {
        throw std::runtime_error("expected 'format' attribute to be a scalar");
    }
    if (!ritsuko::hdf5::is_utf8_string(attr)) {
        throw std::runtime_error("expected 'format' to have a datatype that can be represented by a UTF-8 encoded string");
    }

    return ritsuko::hdf5::load_scalar_string_attribute(attr);
}

inline void validate_string_format(const H5::DataSet& handle, hsize_t len, const std::string& format, bool has_missing, const std::string& missing_value, hsize_t buffer_size) {
    if (format == "date") {
        ritsuko::hdf5::Stream1dStringDataset stream(&handle, len, buffer_size);
        for (hsize_t i = 0; i < len; ++i, stream.next()) {
            auto x = stream.steal();
            if (has_missing && missing_value == x) {
                continue;
            }
            if (!ritsuko::is_date(x.c_str(), x.size())) {
                throw std::runtime_error("expected a date-formatted string (got '" + x + "')");
            }
        }

    } else if (format == "date-time") {
        ritsuko::hdf5::Stream1dStringDataset stream(&handle, len, buffer_size);
        for (hsize_t i = 0; i < len; ++i, stream.next()) {
            auto x = stream.steal();
            if (has_missing && missing_value == x) {
                continue;
            }
            if (!ritsuko::is_rfc3339(x.c_str(), x.size())) {
                throw std::runtime_error("expected a date/time-formatted string (got '" + x + "')");
            }
        }

    } else if (format == "none") {
        ritsuko::hdf5::validate_1d_string_dataset(handle, len, buffer_size);

    } else {
        throw std::runtime_error("unsupported format '" + format + "'");
    }
}

inline void validate_names(const H5::Group& handle, const std::string& name, size_t len, hsize_t buffer_size) {
    if (!handle.exists(name)) {
        return;
    }

    auto nhandle = ritsuko::hdf5::open_dataset(handle, name.c_str());
    if (!ritsuko::hdf5::is_utf8_string(nhandle)) {
        throw std::runtime_error("expected '" + name + "' to have a datatype that can be represented by a UTF-8 encoded string");
    }

    auto nlen = ritsuko::hdf5::get_1d_length(nhandle.getSpace(), false);
    if (len != nlen) {
        throw std::runtime_error("'" + name + "' should have the same length as the parent object (got " + std::to_string(nlen) + ", expected " + std::to_string(len) + ")");
    }

    ritsuko::hdf5::validate_1d_string_dataset(nhandle, len, buffer_size);
}

}

}

#endif
