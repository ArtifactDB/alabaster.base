#ifndef TAKANE_BCF_FILE_HPP
#define TAKANE_BCF_FILE_HPP

#include "utils_files.hpp"

#include "ritsuko/ritsuko.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

/**
 * @file bcf_file.hpp
 * @brief Validation for BCF files.
 */

namespace takane {

/**
 * @namespace takane::bcf_file
 * @brief Definitions for BCF files.
 */
namespace bcf_file {

/**
 * If `Options::bcf_file_strict_check` is provided, it is used to perform stricter checking of the BCF file contents and indices.
 * By default, we don't look past the magic number to verify the files as this requires a dependency on heavy-duty libraries like, e.g., HTSlib.
 *
 * @param path Path to the directory containing the BCF file.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    const std::string& vstring = internal_json::extract_version_for_type(metadata.other, "bcf_file");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    // Magic number taken from https://samtools.github.io/hts-specs/BCFv2_qref.pdf
    // We relax it a little to support both BCF1 and BCF2+ formats.
    auto ipath = path / "file.bcf";
    internal_files::check_gzip_signature(ipath);
    internal_files::check_signature<byteme::GzipFileReader>(ipath, "BCF", 3, "BCF");

    // Magic number taken from https://samtools.github.io/hts-specs/tabix.pdf
    auto ixpath = ipath;
    ixpath += ".tbi";
    if (std::filesystem::exists(ixpath)) {
        internal_files::check_gzip_signature(ixpath);
        internal_files::check_signature<byteme::GzipFileReader>(ixpath, "TBI\1", 4, "tabix");
    }

    // Magic number taken from https://samtools.github.io/hts-specs/CSIv1.pdf
    ixpath = ipath;
    ixpath += ".csi";
    if (std::filesystem::exists(ixpath)) {
        internal_files::check_gzip_signature(ixpath);
        internal_files::check_signature<byteme::GzipFileReader>(ixpath, "CSI\1", 4, "CSI index");
    }

    if (options.bcf_file_strict_check) {
        options.bcf_file_strict_check(path, metadata, options);
    }
}

}

}

#endif
