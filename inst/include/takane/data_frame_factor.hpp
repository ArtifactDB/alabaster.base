#ifndef TAKANE_DATA_FRAME_FACTOR_HPP
#define TAKANE_DATA_FRAME_FACTOR_HPP

#include <string>
#include <stdexcept>
#include <filesystem>

#include "ritsuko/hdf5/hdf5.hpp"

#include "utils_public.hpp"
#include "utils_string.hpp"
#include "utils_factor.hpp"
#include "utils_json.hpp"
#include "utils_other.hpp"

/**
 * @file data_frame_factor.hpp
 * @brief Validation for data frame factors.
 */

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, const ObjectMetadata&, const Options&);
size_t height(const std::filesystem::path&, const ObjectMetadata&, const Options&);
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
 * This should accept a path to the directory containing the data frame, the object metadata, and additional reading options.
 * It should return a boolean indicating whether any duplicate rows were found. 
 *
 * If provided, this enables stricter checking of the uniqueness of the data frame levels.
 * Currently, we don't provide a default method for `data_frame` objects, as it's kind of tedious and we haven't gotten around to it yet.
 */
inline std::function<bool(const std::filesystem::path&, const ObjectMetadata&, const Options& options)> any_duplicated;

/**
 * @param path Path to the directory containing the data frame factor.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, const Options& options) {
    const auto& vstring = internal_json::extract_version_for_type(metadata.other, "data_frame_factor");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    // Validating the levels.
    auto lpath = path / "levels";
    auto lmeta = read_object_metadata(lpath);
    if (!satisfies_interface(lmeta.type, "DATA_FRAME")) {
        throw std::runtime_error("expected 'levels' to be an object that satifies the 'DATA_FRAME' interface");
    }

    try {
        ::takane::validate(lpath, lmeta, options);
    } catch (std::exception& e) {
        throw std::runtime_error("failed to validate 'levels'; " + std::string(e.what()));
    }
    size_t num_levels = ::takane::height(lpath, lmeta, options);

    if (any_duplicated) {
        if (any_duplicated(lpath, lmeta, options)) {
            throw std::runtime_error("'levels' should not contain duplicated rows");
        }
    }

    auto handle = ritsuko::hdf5::open_file(path / "contents.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "data_frame_factor");
    size_t num_codes = internal_factor::validate_factor_codes(ghandle, "codes", num_levels, options.hdf5_buffer_size, /* allow_missing = */ false);

    internal_other::validate_mcols(path, "element_annotations", num_codes, options);
    internal_other::validate_metadata(path, "other_annotations", options);

    internal_string::validate_names(ghandle, "names", num_codes, options.hdf5_buffer_size);
}

/**
 * @param path Path to the directory containing the data frame factor.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, typically for reading performance.
 * @return Length of the factor.
 */
inline size_t height(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] const Options& options) {
    auto handle = ritsuko::hdf5::open_file(path / "contents.h5");
    auto ghandle = handle.openGroup("data_frame_factor");
    auto dhandle = ghandle.openDataSet("codes");
    return ritsuko::hdf5::get_1d_length(dhandle.getSpace(), false);
}

}

}

#endif
