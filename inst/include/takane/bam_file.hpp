#ifndef TAKANE_BAM_FILE_HPP
#define TAKANE_BAM_FILE_HPP

#include "utils_files.hpp"

#include "ritsuko/ritsuko.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace takane {

/**
 * @namespace takane::bam_file
 * @brief Definitions for BAM files.
 */
namespace bam_file {

/**
 * Application-specific function to check the validity of a BAM file and its indices.
 *
 * This should accept a path to the directory containing the BAM file and indices, the object metadata, and additional reading options.
 * It should throw an error if the BAM file is not valid, e.g., corrupted file, mismatched indices.
 *
 * If provided, this enables stricter checking of the BAM file contents and indices.
 * Currently, we don't look past the magic number to verify the files as this requires a dependency on heavy-duty libraries like, e.g., HTSlib.
 */
inline std::function<void(const std::filesystem::path&, const ObjectMetadata&, const Options&)> strict_check;

/**
 * @param path Path to the directory containing the BAM file.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, [[maybe_unused]] const Options& options) {
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

    if (strict_check) {
        strict_check(path, metadata, options);
    }
}

}

}

#endif
