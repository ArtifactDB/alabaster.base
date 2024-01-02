#ifndef TAKANE_FASTA_FILE_HPP
#define TAKANE_FASTA_FILE_HPP

#include "utils_files.hpp"
#include "ritsuko/ritsuko.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace takane {

/**
 * @namespace takane::fasta_file
 * @brief Definitions for FASTA files.
 */
namespace fasta_file {

/**
 * Application-specific function to check the validity of a FASTA file and its indices.
 *
 * This should accept a path to the directory containing the FASTA file, the object metadata, additional reading options,
 * and a boolean indicating whether indices are expected to be present in the directory.
 * It should throw an error if the FASTA file is not valid, e.g., corrupted file, mismatched indices.
 *
 * If provided, this enables stricter checking of the FASTA file contents and indices.
 * Currently, we don't look past the magic number to verify the files as this requires a dependency on heavy-duty libraries like, e.g., HTSlib.
 */
inline std::function<void(const std::filesystem::path&, const ObjectMetadata&, const Options&, bool)> strict_check;

/**
 * @param path Path to the directory containing the FASTA file.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, [[maybe_unused]] const Options& options) {
    const auto& famap = internal_json::extract_typed_object_from_metadata(metadata.other, "fasta_file");

    const std::string& vstring = internal_json::extract_string_from_typed_object(famap, "version", "fasta_file");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    internal_files::check_sequence_type(famap, "fasta_file");

    // Check if it's indexed.
    bool indexed = internal_files::is_indexed(famap);
    auto fpath = path / "file.fasta.";
    if (indexed) {
        fpath += "bgz";
    } else {
        fpath += "gz";
    }

    internal_files::check_gzip_signature(fpath);
    auto reader = internal_other::open_reader<byteme::GzipFileReader>(fpath, 10);
    byteme::PerByte<> pb(&reader);
    if (!pb.valid() || pb.get() != '>') {
        throw std::runtime_error("FASTA file does not start with '>'");
    }

    if (indexed) {
        auto fixpath = path / "file.fasta.fai";
        if (!std::filesystem::exists(fixpath)) {
            throw std::runtime_error("missing FASTA index file");
        }

        auto ixpath = fpath;
        ixpath += ".gzi";
        if (!std::filesystem::exists(ixpath)) {
            throw std::runtime_error("missing BGZF index file");
        }
    }

    if (strict_check) {
        strict_check(path, metadata, options, indexed);
    }
}

}

}

#endif
