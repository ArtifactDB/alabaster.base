#ifndef TAKANE_GMT_FILE_HPP
#define TAKANE_GMT_FILE_HPP

#include "utils_files.hpp"
#include "ritsuko/ritsuko.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace takane {

/**
 * @namespace takane::gmt_file
 * @brief Definitions for GMT files.
 */
namespace gmt_file {

/**
 * Application-specific function to check the validity of a GMT file.
 *
 * This should accept a path to the directory containing the GMT file, the object metadata and additional reading options.
 * It should throw an error if the GMT file is not valid, e.g., corrupted file, mismatched indices.
 *
 * If provided, this enables stricter checking of the GMT file contents.
 * Currently, we don't look past the magic number to verify the files.
 */
inline std::function<void(const std::filesystem::path&, const ObjectMetadata&, const Options&)> strict_check;

/**
 * @param path Path to the directory containing the GMT file.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, [[maybe_unused]] const Options& options) {
    const std::string& vstring = internal_json::extract_version_for_type(metadata.other, "gmt_file");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    auto fpath = path / "file.gmt.gz";
    internal_files::check_gzip_signature(fpath);

    if (strict_check) {
        strict_check(path, metadata, options);
    }
}

}

}

#endif
