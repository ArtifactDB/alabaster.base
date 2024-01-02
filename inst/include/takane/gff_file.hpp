#ifndef TAKANE_GFF_FILE_HPP
#define TAKANE_GFF_FILE_HPP

#include "utils_files.hpp"
#include "ritsuko/ritsuko.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

namespace takane {

/**
 * @namespace takane::gff_file
 * @brief Definitions for GFF files.
 */
namespace gff_file {

/**
 * Application-specific function to check the validity of a GFF file.
 *
 * This should accept a path to the directory containing the GFF file, the object metadata, additional reading options.
 * and a boolean indicating whether indices are expected to be present in the directory.
 * It should throw an error if the GFF file is not valid, e.g., corrupted file, mismatched indices.
 *
 * If provided, this enables stricter checking of the GFF file contents.
 * Currently, we don't look past the magic number to verify the files.
 */
inline std::function<void(const std::filesystem::path&, const ObjectMetadata&, const Options&, bool)> strict_check;

/**
 * @param path Path to the directory containing the GFF file.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, [[maybe_unused]] const Options& options) {
    const auto& gffmap = internal_json::extract_typed_object_from_metadata(metadata.other, "gff_file");

    const std::string& vstring = internal_json::extract_string_from_typed_object(gffmap, "version", "gff_file");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    auto fpath = path / "file.";

    const std::string& fstring = internal_json::extract_string_from_typed_object(gffmap, "format", "gff_file");
    if (fstring == "GFF2") {
        fpath += "gff2";
    } else if (fstring == "GFF3") {
        fpath += "gff3";
    } else {
        throw std::runtime_error("unknown value '" + fstring + "' for 'gff_file.format' property");
    }

    // Check if it's indexed.
    bool indexed = internal_files::is_indexed(gffmap);
    fpath += ".";
    if (indexed) {
        fpath += "bgz";
    } else {
        fpath += "gz";
    }

    // Check magic numbers.
    internal_files::check_gzip_signature(fpath);

    if (fstring == "GFF3") {
        const std::string expected = "##gff-version 3";
        const size_t expected_len = expected.size();

        auto reader = internal_other::open_reader<byteme::GzipFileReader>(fpath, expected_len);
        byteme::PerByte<> pb(&reader);
        bool okay = pb.valid();

        for (size_t i = 0; i < expected_len; ++i) {
            if (!okay) {
                throw std::runtime_error("incomplete GFF3 file signature for '" + fpath.string() + "'");
            }
            if (pb.get() != expected[i]) {
                throw std::runtime_error("incorrect GFF3 file signature for '" + fpath.string() + "'");
            }
            okay = pb.advance();
        }
    }

    if (indexed) {
        auto ixpath = fpath;
        ixpath += ".tbi";
        internal_files::check_gzip_signature(ixpath);
        internal_files::check_signature<byteme::GzipFileReader>(ixpath, "TBI\1", 4, "tabix");
    }

    if (strict_check) {
        strict_check(path, metadata, options, indexed);
    }
}

}

}

#endif
