#ifndef TAKANE_BAM_FILE_HPP
#define TAKANE_BAM_FILE_HPP

#include "utils_files.hpp"

#include "ritsuko/ritsuko.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

/**
 * @file bam_file.hpp
 * @brief Validation for BAM files.
 */

namespace takane {

/**
 * @namespace takane::bam_file
 * @brief Definitions for BAM files.
 */
namespace bam_file {

/**
 * If `Options::bam_file_strict_check` is provided, it is used to perform stricter checking of the BAM file contents and indices.
 * By default, we don't look past the magic number to verify the files as this requires a dependency on heavy-duty libraries like, e.g., HTSlib.
 *
 * @param path Path to the directory containing the BAM file.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    const std::string& vstring = internal_json::extract_version_for_type(metadata.other, "bam_file");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    // Magic numbers taken from https://samtools.github.io/hts-specs/SAMv1.pdf
    auto ipath = path / "file.bam";
    internal_files::check_gzip_signature(ipath);
    internal_files::check_signature<byteme::GzipFileReader>(ipath, "BAM\1", 4, "BAM");

    auto ixpath = ipath;
    ixpath += ".bai";
    if (std::filesystem::exists(ixpath)) {
        internal_files::check_signature(ixpath, "BAI\1", 4, "BAM index");
    }

    // Magic number taken from https://samtools.github.io/hts-specs/CSIv1.pdf
    ixpath = ipath;
    ixpath += ".csi";
    if (std::filesystem::exists(ixpath)) {
        internal_files::check_gzip_signature(ixpath);
        internal_files::check_signature<byteme::GzipFileReader>(ixpath, "CSI\1", 4, "CSI index");
    }

    if (options.bam_file_strict_check) {
        options.bam_file_strict_check(path, metadata, options);
    }
}

}

}

#endif
