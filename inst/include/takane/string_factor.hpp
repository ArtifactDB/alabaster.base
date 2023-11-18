#ifndef TAKANE_STRING_FACTOR_HPP
#define TAKANE_STRING_FACTOR_HPP

#include <string>
#include <stdexcept>
#include <filesystem>

#include "ritsuko/hdf5/hdf5.hpp"

#include "utils_public.hpp"
#include "utils_hdf5.hpp"

/**
 * @file string_factor.hpp
 * @brief Validation for string factors.
 */

namespace takane {

/**
 * @namespace takane::string_factor
 * @brief Definitions for string factors.
 */
namespace string_factor {

/**
 * @param path Path to the directory containing the string factor.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const Options& options) try {
    H5::H5File handle((path / "contents.h5").string(), H5F_ACC_RDONLY);

    const char* parent = "string_factor";
    if (!handle.exists(parent) || handle.childObjType(parent) != H5O_TYPE_GROUP) {
        throw std::runtime_error("expected a 'string_factor' group");
    }
    auto ghandle = handle.openGroup(parent);

    auto vstring = ritsuko::hdf5::load_scalar_string_attribute(ghandle, "version");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    if (ghandle.attrExists("ordered")) {
        auto oattr = ritsuko::hdf5::get_scalar_attribute(ghandle, "ordered");
        if (ritsuko::hdf5::exceeds_integer_limit(oattr, 32, true)) {
            throw std::runtime_error("expected a datatype for the 'ordered' attribute that fits in a 32-bit signed integer");
        }
    }

    // Number of levels.
    size_t num_levels = internal_hdf5::validate_factor_levels(ghandle, "levels", options.hdf5_buffer_size);
    size_t num_codes = internal_hdf5::validate_factor_codes(ghandle, "codes", num_levels, options.hdf5_buffer_size);

    if (ghandle.exists("names")) {
        auto nhandle = ritsuko::hdf5::get_dataset(ghandle, "names");
        if (nhandle.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("'names' should be a string datatype class");
        }
        auto nlen = ritsuko::hdf5::get_1d_length(nhandle.getSpace(), false);
        if (num_codes != nlen) {
            throw std::runtime_error("'names' and 'codes' should have the same length");
        }
    }

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate a 'string_factor' at '" + path.string() + "'; " + std::string(e.what()));
}

/**
 * Overload of `string_factor::validate()` with default options.
 * @param path Path to the directory containing the string factor.
 */
inline void validate(const std::filesystem::path& path) {
    string_factor::validate(path, Options());
}

}

}

#endif
