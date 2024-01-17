#ifndef TAKANE_VCF_EXPERIMENT_HPP
#define TAKANE_VCF_EXPERIMENT_HPP

#include <string>
#include <stdexcept>
#include <filesystem>

#include "ritsuko/hdf5/hdf5.hpp"

#include "utils_public.hpp"
#include "utils_string.hpp"
#include "utils_summarized_experiment.hpp"
#include "utils_json.hpp"
#include "utils_files.hpp"

/**
 * @file vcf_experiment.hpp
 * @brief Validation for VCF-based experiments.
 */

namespace takane {

/**
 * @namespace takane::vcf_experiment
 * @brief Definitions for VCF experiments.
 */
namespace vcf_experiment {

/**
 * @cond
 */
namespace internal {

// Format specification taken from https://samtools.github.io/hts-specs/VCFv4.1.pdf.
template<bool parallel_>
std::pair<size_t, size_t> scan_vcf_dimensions(const std::filesystem::path& path, bool expanded) {
    internal_files::check_gzip_signature(path);
    auto reader = internal_other::open_reader<byteme::GzipFileReader>(path);
    typename std::conditional<parallel_, byteme::PerByteParallel<>, byteme::PerByte<> >::type pb(&reader);

    // Checking the signature.
    {
        const std::string expected = "##fileformat=VCFv";
        const size_t len = expected.size();
        bool okay = pb.valid();

        for (size_t i = 0; i < len; ++i) {
            if (!okay) {
                throw std::runtime_error("incomplete VCF file signature");
            }
            if (pb.get() != expected[i]) {
                throw std::runtime_error("incorrect VCF file signature");
            }
            okay = pb.advance();
        }
    }

    // Scanning until we find a line that doesn't start with '##'.
    while (true) {
        if (pb.get() == '\n') {
            bool escape = false;
            for (int i = 0; i < 2; ++i) {
                if (!pb.advance()) {
                    throw std::runtime_error("premature end to the VCF file");
                }
                if (pb.get() != '#') {
                    escape = true;
                    break;
                }
            }
            if (escape) {
                break;
            }
        }
        if (!pb.advance()) {
            throw std::runtime_error("premature end to the VCF file");
        }
    }

    // Scanning the header line to count the number of samples.
    size_t num_samples = 0;
    {
        size_t num_indents = 0;
        while (true) {
            char current = pb.get();
            if (current == '\t') {
                ++num_indents;
            } else if (current == '\n') {
                pb.advance(); // skip past the newline.
                break;
            }
            if (!pb.advance()) {
                throw std::runtime_error("premature end to the VCF file");
            }
        }

        if (num_indents < 8) {
            throw std::runtime_error("expected at least 9 fields in the VCF header line, including 'FORMAT'");
        }
        num_samples = num_indents - 8;
    }

    size_t expected_rows = 0;
    if (expanded) {
        while (pb.valid()) {
            ++expected_rows;

            // Scanning up to the ALT field, which is the 5th one. We need this to
            // find the number of alternative alleles (and thus the expansion of rows).
            size_t num_indents = 0;
            while (true) {
                char current = pb.get();
                if (current == '\t') {
                    ++num_indents;
                    if (num_indents == 4) { 
                        if (!pb.advance()) { // get past this indent.
                            throw std::runtime_error("premature end of line for VCF record");
                        }
                        break;
                    }
                } else if (current == '\n') {
                    throw std::runtime_error("premature end of line for VCF record");
                }
                if (!pb.advance()) {
                    throw std::runtime_error("premature end of line for VCF record");
                }
            }

            // Checking that we don't have any commas if it's expanded.
            while (true) {
                char current = pb.get();
                if (current == ',') {
                    throw std::runtime_error("expected a 1:1 mapping of rows to alternative alleles when 'vcf_experiment.expanded = true'");
                } else if (current == '\t') {
                    break;
                } else if (current == '\n') {
                    throw std::runtime_error("premature end of line for VCF record");
                }
                if (!pb.advance()) {
                    throw std::runtime_error("premature end of line for VCF record");
                }
            }

            // Chomping the rest of the line; we assume all lines are newline-terminated.
            while (true) {
                if (pb.get() == '\n') {
                    pb.advance(); // skip past the newline.
                    break;
                } else {
                    if (!pb.advance()) {
                        throw std::runtime_error("premature end of line for VCF record");
                    }
                }
            }
        }

    } else {
        if (pb.valid()) {
            while (true) {
                if (pb.get() == '\n') {
                    ++expected_rows; // assume all files are newline-terminated.
                    if (!pb.advance()) {
                        break;
                    }
                } else {
                    if (!pb.advance()) {
                        throw std::runtime_error("premature end of line for VCF record");
                    }
                }
            }
        }
    }

    return std::make_pair(expected_rows, num_samples);
}

}
/**
 * @endcond
 */

/**
 * @param path Path to the directory containing the VCF experiment.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    const auto& vcfmap = internal_json::extract_typed_object_from_metadata(metadata.other, "vcf_experiment");

    const std::string& vstring = internal_json::extract_string_from_typed_object(vcfmap, "version", "vcf_experiment");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    // Other bits and pieces of the metadata to check.
    auto dims = internal_summarized_experiment::extract_dimensions_json(vcfmap, "vcf_experiment");

    bool exp = false;
    {
        auto eIt = vcfmap.find("expanded");
        if (eIt == vcfmap.end()) {
            throw std::runtime_error("expected a 'vcf_experiment.expanded' property");
        }
        const auto& val = eIt->second;
        if (val->type() != millijson::BOOLEAN) {
            throw std::runtime_error("'vcf_experiment.expanded' property should be a JSON boolean");
        }
        exp = reinterpret_cast<const millijson::Boolean*>(val.get())->value;
    }

    auto ipath = path / "file.vcf.gz";
    std::pair<size_t, size_t> obs_dims;
    try {
        if (options.parallel_reads) {
            obs_dims = internal::scan_vcf_dimensions<true>(ipath, exp);
        } else {
            obs_dims = internal::scan_vcf_dimensions<false>(ipath, exp);
        }
    } catch (std::exception& e) {
        throw std::runtime_error("failed to parse '" + ipath.string() + "'; " + std::string(e.what()));
    }

    if (obs_dims.first != dims.first) {
        throw std::runtime_error("reported 'vcf_experiment.dimensions[0]' does not match the number of records in '" + ipath.string() + "'");
    }
    if (obs_dims.second != dims.second) {
        throw std::runtime_error("reported 'vcf_experiment.dimensions[1]' does not match the number of samples in '" + ipath.string() + "'");
    }
}

/**
 * @param path Path to a directory containing a VCF experiment.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return Number of rows in the VCF experiment.
 */
inline size_t height([[maybe_unused]] const std::filesystem::path& path, const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    const auto& vcfmap = internal_json::extract_typed_object_from_metadata(metadata.other, "vcf_experiment");
    auto dims = internal_summarized_experiment::extract_dimensions_json(vcfmap, "vcf_experiment");
    return dims.first;
}

/**
 * @param path Path to a directory containing a VCF experiment.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return A vector of length 2 containing the dimensions of the VCF experiment.
 */
inline std::vector<size_t> dimensions([[maybe_unused]] const std::filesystem::path& path, const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    const auto& vcfmap = internal_json::extract_typed_object_from_metadata(metadata.other, "vcf_experiment");
    auto dims = internal_summarized_experiment::extract_dimensions_json(vcfmap, "vcf_experiment");
    return std::vector<size_t>{ dims.first, dims.second };
}

}

}

#endif
