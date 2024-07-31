#ifndef TAKANE_RDS_FILE_HPP
#define TAKANE_RDS_FILE_HPP

#include "utils_files.hpp"
#include "ritsuko/ritsuko.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

/**
 * @file rds_file.hpp
 * @brief Validation for RDS files.
 */

namespace takane {

/**
 * @namespace takane::rds_file
 * @brief Definitions for RDS files.
 */
namespace rds_file {

/**
 * If `Options::rds_file_strict_check` is provided, this enables stricter checking of the RDS file contents.
 * By default, we just look at the first few bytes to verify the files. 
 *
 * @param path Path to the directory containing the RDS file.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    const auto& rdsmap = internal_json::extract_typed_object_from_metadata(metadata.other, "rds_file");

    const std::string& vstring = internal_json::extract_string_from_typed_object(rdsmap, "version", "rds_file");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    auto fpath = path / "file.rds";

    // Check magic numbers.
    internal_files::check_gzip_signature(fpath);

    const char* expected = "X\n";
    size_t expected_len = 2;

    auto reader = internal_other::open_reader<byteme::GzipFileReader>(fpath, expected_len);
    byteme::PerByte<> pb(&reader);
    bool okay = pb.valid();

    for (size_t i = 0; i < expected_len; ++i) {
        if (!okay) {
            throw std::runtime_error("incomplete RDS file signature for '" + fpath.string() + "'");
        }
        if (pb.get() != expected[i]) {
            throw std::runtime_error("incorrect RDS file signature for '" + fpath.string() + "'");
        }
        okay = pb.advance();
    }

    if (options.rds_file_strict_check) {
        options.rds_file_strict_check(path, metadata, options);
    }
}

}

}

#endif
