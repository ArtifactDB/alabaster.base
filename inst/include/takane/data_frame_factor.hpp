#ifndef TAKANE_DATA_FRAME_FACTOR_HPP
#define TAKANE_DATA_FRAME_FACTOR_HPP

#include <string>
#include <stdexcept>
#include <filesystem>

#include "ritsuko/hdf5/hdf5.hpp"

#include "utils_public.hpp"
#include "utils_string.hpp"
#include "utils_factor.hpp"

/**
 * @file data_frame_factor.hpp
 * @brief Validation for data frame factors.
 */

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, const std::string&, const Options&);
size_t height(const std::filesystem::path&, const std::string&, const Options&);
bool satisfies_interface(const std::string&, const std::string&);
/**
 * @endcond
 */

/**
 * @namespace takane::data_frame_factor
 * @brief Definitions for data frame factors.
 */
namespace data_frame_factor {

/**
 * Application-specific function to determine whether there are duplicated rows in the data frame containing the levels of a `data_frame_factor`.
 *
 * This should accept a path to the directory containing the data frame, a string specifying the object type, and additional reading options.
 * It should return a boolean indicating whether any duplicate rows were found. 
 *
 * If provided, this enables stricter checking of the uniqueness of the data frame levels.
 * Currently, we don't provide a default method for `data_frame` objects, as it's kind of tedious and we haven't gotten around to it yet.
 */
inline std::function<bool(const std::filesystem::path&, const std::string&, const Options& options)> any_duplicated;

/**
 * @param path Path to the directory containing the data frame factor.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const Options& options) try {
    auto handle = ritsuko::hdf5::open_file(path / "contents.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "data_frame_factor");

    auto vstring = ritsuko::hdf5::open_and_load_scalar_string_attribute(ghandle, "version");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    // Validating the levels.
    auto lpath = path / "levels";
    auto xtype = read_object_type(lpath);
    if (!satisfies_interface(xtype, "DATA_FRAME")) {
        throw std::runtime_error("expected 'levels' to be an object that satifies the 'DATA_FRAME' interface");
    }

    try {
        ::takane::validate(lpath, xtype, options);
    } catch (std::exception& e) {
        throw std::runtime_error("failed to validate 'levels'; " + std::string(e.what()));
    }
    size_t num_levels = ::takane::height(lpath, xtype, options);

    if (any_duplicated) {
        if (any_duplicated(lpath, xtype, options)) {
            throw std::runtime_error("'levels' should not contain duplicated rows");
        }
    }

    size_t num_codes = internal_factor::validate_factor_codes(ghandle, "codes", num_levels, options.hdf5_buffer_size, /* allow_missing = */ false);

    internal_other::validate_mcols(path, "element_annotations", num_codes, options);
    internal_other::validate_metadata(path, "other_annotations", options);

    internal_string::validate_names(ghandle, "names", num_codes, options.hdf5_buffer_size);

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate a 'data_frame_factor' at '" + path.string() + "'; " + std::string(e.what()));
}

/**
 * @param path Path to the directory containing the data frame factor.
 * @param options Validation options, typically for reading performance.
 * @return Length of the factor.
 */
inline size_t height(const std::filesystem::path& path, const Options&) {
    H5::H5File handle(path / "contents.h5", H5F_ACC_RDONLY);
    auto ghandle = handle.openGroup("data_frame_factor");
    auto dhandle = ghandle.openDataSet("codes");
    return ritsuko::hdf5::get_1d_length(dhandle.getSpace(), false);
}

}

}

#endif
