#ifndef TAKANE_ATOMIC_VECTOR_HPP
#define TAKANE_ATOMIC_VECTOR_HPP

#include <string>
#include <stdexcept>
#include <filesystem>

#include "ritsuko/hdf5/hdf5.hpp"

#include "utils_public.hpp"
#include "utils_string.hpp"

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
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const Options& options) try {
    auto handle = ritsuko::hdf5::open_file(path / "contents.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "atomic_vector");

    auto vstring = ritsuko::hdf5::open_and_load_scalar_string_attribute(ghandle, "version");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    auto dhandle = ritsuko::hdf5::open_dataset(ghandle, "values");
    auto vlen = ritsuko::hdf5::get_1d_length(dhandle.getSpace(), false);
    auto type = ritsuko::hdf5::open_and_load_scalar_string_attribute(ghandle, "type");

    const char* missing_attr_name = "missing-value-placeholder";

    if (type == "string") {
        if (dhandle.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("expected a string datatype for 'values'");
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

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate an 'atomic_vector' at '" + path.string() + "'; " + std::string(e.what()));
}

/**
 * @param path Path to the directory containing the atomic vector.
 * @param options Validation options, typically for reading performance.
 * @return Length of the vector.
 */
inline size_t height(const std::filesystem::path& path, const Options&) {
    H5::H5File handle((path / "contents.h5").string(), H5F_ACC_RDONLY);
    auto ghandle = handle.openGroup("atomic_vector");
    auto dhandle = ghandle.openDataSet("values");
    return ritsuko::hdf5::get_1d_length(dhandle.getSpace(), false);
}

}

}

#endif
