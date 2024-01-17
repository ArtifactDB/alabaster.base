#ifndef TAKANE_FASTQ_FILE_HPP
#define TAKANE_FASTQ_FILE_HPP

#include "utils_files.hpp"
#include "ritsuko/ritsuko.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

/**
 * @file fastq_file.hpp
 * @brief Validation for FASTQ files.
 */

namespace takane {

/**
 * @namespace takane::fastq_file
 * @brief Definitions for FASTQ files.
 */
namespace fastq_file {

/**
 * If `Options::fastq_file_strict_check` is provided, this enables stricter checking of the FASTQ file contents and indices.
 * By default, we just look at the first few bytes to verify the files.
 *
 * @param path Path to the directory containing the FASTQ file.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    const auto& fqmap = internal_json::extract_typed_object_from_metadata(metadata.other, "fastq_file");

    const std::string& vstring = internal_json::extract_string_from_typed_object(fqmap, "version", "fastq_file");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    internal_files::check_sequence_type(fqmap, "fastq_file");

    // Checking the quality type and offset.
    {
        const std::string& qtype = internal_json::extract_string(fqmap, "quality_type", [&](std::exception& e) -> void {
            throw std::runtime_error("failed to extract 'fastq_file.quality_type' from the object metadata; " + std::string(e.what())); 
        });

        if (qtype == "phred") {
            auto oIt = fqmap.find("quality_offset");
            if (oIt == fqmap.end()) {
                throw std::runtime_error("expected a 'fastq_file.quality_offset' property");
            }

            const auto& val = oIt->second;
            if (val->type() != millijson::NUMBER) {
                throw std::runtime_error("'fastq_file.quality_offset' property should be a JSON number");
            }

            double offset = reinterpret_cast<const millijson::Number*>(val.get())->value;
            if (offset != 33 && offset != 64) {
                throw std::runtime_error("'fastq_file.quality_offset' property should be either 33 or 64");
            }
        } else if (qtype != "solexa") {
            throw std::runtime_error("unknown value '" + qtype + "' for the 'fastq_file.quality_type' property");
        }
    }

    // Check if it's indexed.
    bool indexed = internal_files::is_indexed(fqmap);
    auto fpath = path / "file.fastq.";
    if (indexed) {
        fpath += "bgz";
    } else {
        fpath += "gz";
    }

    internal_files::check_gzip_signature(fpath);
    auto reader = internal_other::open_reader<byteme::GzipFileReader>(fpath, 10);
    byteme::PerByte<> pb(&reader);
    if (!pb.valid() || pb.get() != '@') {
        throw std::runtime_error("FASTQ file does not start with '@'");
    }

    if (indexed) {
        auto fixpath = path / "file.fastq.fai";
        if (!std::filesystem::exists(fixpath)) {
            throw std::runtime_error("missing FASTQ index file");
        }

        auto ixpath = fpath;
        ixpath += ".gzi";
        if (!std::filesystem::exists(ixpath)) {
            throw std::runtime_error("missing BGZF index file");
        }
    }

    if (options.fastq_file_strict_check) {
        options.fastq_file_strict_check(path, metadata, options, indexed);
    }
}

}

}

#endif
