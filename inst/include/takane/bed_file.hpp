#ifndef TAKANE_BED_FILE_HPP
#define TAKANE_BED_FILE_HPP

#include "utils_files.hpp"

#include "ritsuko/ritsuko.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

/**
 * @file bed_file.hpp
 * @brief Validation for BED files.
 */

namespace takane {

/**
 * @namespace takane::bed_file
 * @brief Definitions for BED files.
 */
namespace bed_file {

/**
 * If `Options::bed_file_strict_check` is provided, it is used to perform stricter checking of the BED file contents and indices.
 * Currently, we don't look past the magic number to verify the files as this requires a dependency on heavy-duty libraries like, e.g., HTSlib.
 *
 * @param path Path to the directory containing the BED file.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    const auto& bedmap = internal_json::extract_typed_object_from_metadata(metadata.other, "bed_file");

    const std::string& vstring = internal_json::extract_string_from_typed_object(bedmap, "version", "bed_file");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    // Check if it's indexed.
    bool indexed = internal_files::is_indexed(bedmap);
    auto fpath = path / "file.bed.";
    if (indexed) {
        fpath += "bgz";
    } else {
        fpath += "gz";
    }

    internal_files::check_gzip_signature(fpath);

    if (indexed) {
        auto ixpath = fpath;
        ixpath += ".tbi";
        internal_files::check_gzip_signature(ixpath);
        internal_files::check_signature<byteme::GzipFileReader>(ixpath, "TBI\1", 4, "tabix");
    }

    if (options.bed_file_strict_check) {
        options.bed_file_strict_check(path, metadata, options, indexed);
    }
}

}

}

#endif
