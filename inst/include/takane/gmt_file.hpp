#ifndef TAKANE_GMT_FILE_HPP
#define TAKANE_GMT_FILE_HPP

#include "utils_files.hpp"
#include "ritsuko/ritsuko.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

/**
 * @file gmt_file.hpp
 * @brief Validation for GMT files.
 */

namespace takane {

/**
 * @namespace takane::gmt_file
 * @brief Definitions for GMT files.
 */
namespace gmt_file {

/**
 * If `Options::gmt_file_strict_check` is provided, this enables stricter checking of the GMT file contents.
 * By default, we just look at the first few bytes to verify the files.
 *
 * @param path Path to the directory containing the GMT file.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    const std::string& vstring = internal_json::extract_version_for_type(metadata.other, "gmt_file");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    auto fpath = path / "file.gmt.gz";
    internal_files::check_gzip_signature(fpath);

    if (options.gmt_file_strict_check) {
        options.gmt_file_strict_check(path, metadata, options);
    }
}

}

}

#endif
