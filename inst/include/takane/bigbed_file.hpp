#ifndef TAKANE_BIGBED_FILE_HPP
#define TAKANE_BIGBED_FILE_HPP

#include "utils_files.hpp"

#include "ritsuko/ritsuko.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace takane {

/**
 * @namespace takane::bigbed_file
 * @brief Definitions for bigBed files.
 */
namespace bigbed_file {

/**
 * Application-specific function to check the validity of a bigBed file.
 *
 * This should accept a path to the directory containing the bigBed file, the object metadata, and additional reading options.
 * It should throw an error if the bigBed file is not valid, e.g., corrupted file, mismatched indices.
 *
 * If provided, this enables stricter checking of the bigBed file contents and indices.
 * Currently, we don't look past the magic number to verify the files as this requires a dependency on heavy-duty libraries like, e.g., HTSlib.
 */
inline std::function<void(const std::filesystem::path&, const ObjectMetadata&, const Options&)> strict_check;

/**
 * @param path Path to the directory containing the bigBed file.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, [[maybe_unused]] const Options& options) {
    const std::string& vstring = internal_json::extract_version_for_type(metadata.other, "bigbed_file");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    // Magic numbers taken from https://data.broadinstitute.org/igv/projects/downloads/2.1/IGVDistribution_2.1.11/src/org/broad/igv/bbfile/BBFileHeader.java,
    // which seems to use the magic number to detect endianness of the file as well.
    auto ipath = path / "file.bb";
    std::array<unsigned char, 4> store;
    internal_files::extract_signature(ipath, store.data(), store.size());

    std::array<unsigned char, 4> be_magic { 0xEB, 0xF2, 0x89, 0x87 };
    std::array<unsigned char, 4> le_magic { 0x87, 0x89, 0xF2, 0xEB };
    if (store != be_magic && store != le_magic) {
        throw std::runtime_error("incorrect bigBed file signature for '" + ipath.string() + "'");
    }

    if (strict_check) {
        strict_check(path, metadata, options);
    }
}

}

}

#endif
