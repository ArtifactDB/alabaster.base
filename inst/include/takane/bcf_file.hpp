#ifndef TAKANE_BCF_FILE_HPP
#define TAKANE_BCF_FILE_HPP

#include "utils_files.hpp"

#include "ritsuko/ritsuko.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace takane {

/**
 * @namespace takane::bcf_file
 * @brief Definitions for BCF files.
 */
namespace bcf_file {

/**
 * Application-specific function to check the validity of a BCF file and its indices.
 *
 * This should accept a path to the directory containing the BCF file and indices, the object metadata, and additional reading options.
 * It should throw an error if the BCF file is not valid, e.g., corrupted file, mismatched indices.
 *
 * If provided, this enables stricter checking of the BCF file contents and indices.
 * Currently, we don't look past the magic number to verify the files as this requires a dependency on heavy-duty libraries like, e.g., HTSlib.
 */
inline std::function<void(const std::filesystem::path&, const ObjectMetadata&, const Options&)> strict_check;

/**
 * @param path Path to the directory containing the BCF file.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, [[maybe_unused]] const Options& options) {
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

    if (strict_check) {
        strict_check(path, metadata, options);
    }
}

}

}

#endif
