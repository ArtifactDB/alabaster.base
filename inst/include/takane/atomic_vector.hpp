#ifndef TAKANE_ATOMIC_VECTOR_HPP
#define TAKANE_ATOMIC_VECTOR_HPP

#include <string>
#include <stdexcept>
#include <filesystem>

#include "ritsuko/hdf5/hdf5.hpp"

#include "utils_public.hpp"
#include "utils_string.hpp"
#include "utils_json.hpp"

/**
 * @file atomic_vector.hpp
 * @brief Validation for atomic vectors.
 */

namespace takane {

/**
 * @namespace takane::atomic_vector
 * @brief Definitions for atomic vectors.
 */
namespace atomic_vector {

/**
 * @param path Path to the directory containing the atomic vector.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    const auto& vstring = internal_json::extract_version_for_type(metadata.other, "atomic_vector");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    auto handle = ritsuko::hdf5::open_file(path / "contents.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "atomic_vector");
    auto dhandle = ritsuko::hdf5::open_dataset(ghandle, "values");
    auto vlen = ritsuko::hdf5::get_1d_length(dhandle.getSpace(), false);
    auto type = ritsuko::hdf5::open_and_load_scalar_string_attribute(ghandle, "type");

    const char* missing_attr_name = "missing-value-placeholder";

    if (type == "string") {
        if (!ritsuko::hdf5::is_utf8_string(dhandle)) {
            throw std::runtime_error("expected a datatype for 'values' that can be represented by a UTF-8 encoded string");
        }
        auto missingness = ritsuko::hdf5::open_and_load_optional_string_missing_placeholder(dhandle, missing_attr_name);
        std::string format = internal_string::fetch_format_attribute(ghandle);
        internal_string::validate_string_format(dhandle, vlen, format, missingness.first, missingness.second, options.hdf5_buffer_size);

    } else {
        if (type == "integer") {
            if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 32, true)) {
                throw std::runtime_error("expected a datatype for 'values' that fits in a 32-bit signed integer");
            }
        } else if (type == "boolean") {
            if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 32, true)) {
                throw std::runtime_error("expected a datatype for 'values' that fits in a 32-bit signed integer");
            }
        } else if (type == "number") {
            if (ritsuko::hdf5::exceeds_float_limit(dhandle, 64)) {
                throw std::runtime_error("expected a datatype for 'values' that fits in a 64-bit float");
            }
        } else {
            throw std::runtime_error("unsupported type '" + type + "'");
        }

        if (dhandle.attrExists(missing_attr_name)) {
            auto missing_attr = dhandle.openAttribute(missing_attr_name);
            ritsuko::hdf5::check_missing_placeholder_attribute(dhandle, missing_attr);
        }
    }

    internal_string::validate_names(ghandle, "names", vlen, options.hdf5_buffer_size);
}

/**
 * @param path Path to the directory containing the atomic vector.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return Length of the vector.
 */
inline size_t height(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    auto handle = ritsuko::hdf5::open_file(path / "contents.h5");
    auto ghandle = handle.openGroup("atomic_vector");
    auto dhandle = ghandle.openDataSet("values");
    return ritsuko::hdf5::get_1d_length(dhandle.getSpace(), false);
}

}

}

#endif
