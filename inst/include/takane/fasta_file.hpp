#ifndef TAKANE_FASTA_FILE_HPP
#define TAKANE_FASTA_FILE_HPP

#include "utils_files.hpp"
#include "ritsuko/ritsuko.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

/**
 * @file fasta_file.hpp
 * @brief Validation for FASTA files.
 */

namespace takane {

/**
 * @namespace takane::fasta_file
 * @brief Definitions for FASTA files.
 */
namespace fasta_file {

/**
 * If `Options::fasta_file_strict_check` is provided, this enables stricter checking of the FASTA file contents and indices.
 * By default, we just look at the first few bytes to verify the files. 
 *
 * @param path Path to the directory containing the FASTA file.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
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

    if (options.fasta_file_strict_check) {
        options.fasta_file_strict_check(path, metadata, options, indexed);
    }
}

}

}

#endif
