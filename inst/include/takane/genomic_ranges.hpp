#ifndef TAKANE_GENOMIC_RANGES_HPP
#define TAKANE_GENOMIC_RANGES_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include <string>
#include <filesystem>
#include <stdexcept>
#include <cstdint>
#include <type_traits>
#include <limits>

#include "utils_string.hpp"
#include "utils_public.hpp"
#include "utils_other.hpp"
#include "utils_json.hpp"

/**
 * @file genomic_ranges.hpp
 * @brief Validation for genomic ranges.
 */

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, const ObjectMetadata&, Options& options);
bool derived_from(const std::string&, const std::string&, const Options& options);
/**
 * @endcond
 */

/**
 * @namespace takane::genomic_ranges
 * @brief Definitions for genomic ranges.
 */
namespace genomic_ranges {

/**
 * @cond
 */
namespace internal {

struct SequenceLimits {
    SequenceLimits(size_t n) : has_circular(n), circular(n), has_seqlen(n), seqlen(n) {}
    std::vector<unsigned char> has_circular, circular, has_seqlen;
    std::vector<uint64_t> seqlen;
};

inline SequenceLimits find_sequence_limits(const std::filesystem::path& path, Options& options) {
    auto smeta = read_object_metadata(path);
    if (!derived_from(smeta.type, "sequence_information", options)) {
        throw std::runtime_error("'sequence_information' directory should contain a 'sequence_information' object");
    }
    ::takane::validate(path, smeta, options);

    auto handle = ritsuko::hdf5::open_file(path / "info.h5");
    auto ghandle = handle.openGroup("sequence_information");

    auto lhandle = ghandle.openDataSet("length");
    auto num_seq = ritsuko::hdf5::get_1d_length(lhandle.getSpace(), false);
    ritsuko::hdf5::Stream1dNumericDataset<uint64_t> lstream(&lhandle, num_seq, options.hdf5_buffer_size);
    auto lmissing = ritsuko::hdf5::open_and_load_optional_numeric_missing_placeholder<uint64_t>(lhandle, "missing-value-placeholder");

    auto chandle = ghandle.openDataSet("circular");
    ritsuko::hdf5::Stream1dNumericDataset<int32_t> cstream(&chandle, num_seq, options.hdf5_buffer_size);
    auto cmissing = ritsuko::hdf5::open_and_load_optional_numeric_missing_placeholder<int32_t>(chandle, "missing-value-placeholder");

    SequenceLimits output(num_seq);
    for (size_t i = 0; i < num_seq; ++i, lstream.next(), cstream.next()) {
        auto slen = lstream.get();
        auto circ = cstream.get();
        output.has_seqlen[i] = !(lmissing.first && lmissing.second == slen);
        output.seqlen[i] = slen;
        output.has_circular[i] = !(cmissing.first && cmissing.second == circ);
        output.circular[i] = circ;
    }

    return output;
}

}
/**
 * @endcond
 */

/**
 * @param path Path to the directory containing the genomic ranges.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    const auto& vstring = internal_json::extract_version_for_type(metadata.other, "genomic_ranges");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    // Figuring out the sequence length constraints.
    auto limits = internal::find_sequence_limits(path / "sequence_information", options);
    size_t num_sequences = limits.seqlen.size();

    // Now loading all three components.
    auto handle = ritsuko::hdf5::open_file(path / "ranges.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "genomic_ranges");
    auto id_handle = ritsuko::hdf5::open_dataset(ghandle, "sequence");
    auto num_ranges = ritsuko::hdf5::get_1d_length(id_handle, false);
    if (ritsuko::hdf5::exceeds_integer_limit(id_handle, 64, false)) {
        throw std::runtime_error("expected 'sequence' to have a datatype that fits into a 64-bit unsigned integer");
    }
    ritsuko::hdf5::Stream1dNumericDataset<uint64_t> id_stream(&id_handle, num_ranges, options.hdf5_buffer_size);

    auto start_handle = ritsuko::hdf5::open_dataset(ghandle, "start");
    if (num_ranges != ritsuko::hdf5::get_1d_length(start_handle, false)) {
        throw std::runtime_error("'start' and 'sequence' should have the same length");
    }
    if (ritsuko::hdf5::exceeds_integer_limit(start_handle, 64, true)) {
        throw std::runtime_error("expected 'start' to have a datatype that fits into a 64-bit signed integer");
    }
    ritsuko::hdf5::Stream1dNumericDataset<int64_t> start_stream(&start_handle, num_ranges, options.hdf5_buffer_size);

    auto width_handle = ritsuko::hdf5::open_dataset(ghandle, "width");
    if (num_ranges != ritsuko::hdf5::get_1d_length(width_handle, false)) {
        throw std::runtime_error("'width' and 'sequence' should have the same length");
    }
    if (ritsuko::hdf5::exceeds_integer_limit(width_handle, 64, false)) {
        throw std::runtime_error("expected 'width' to have a datatype that fits into a 64-bit unsigned integer");
    }
    ritsuko::hdf5::Stream1dNumericDataset<uint64_t> width_stream(&width_handle, num_ranges, options.hdf5_buffer_size);

    constexpr uint64_t end_limit = std::numeric_limits<int64_t>::max();
    for (size_t i = 0; i < num_ranges; ++i, id_stream.next(), start_stream.next(), width_stream.next()) {
        auto id = id_stream.get();
        if (id >= num_sequences) {
            throw std::runtime_error("'sequence' must be less than the number of sequences (got " + std::to_string(id) + ")");
        }

        auto start = start_stream.get();
        auto width = width_stream.get();

        // If it's definitely non-circular, the start position should be positive.
        if (limits.has_circular[id] && !limits.circular[id]) {
            if (start < 1) {
                throw std::runtime_error("non-positive start position (" + std::to_string(start) + ") for non-circular sequence");
            }

            if (limits.has_seqlen[id]) {
                // If the sequence length is provided, the end position shouldn't overflow.
                auto spos = static_cast<uint64_t>(start);
                auto limit = limits.seqlen[id];
                if (spos > limit) {
                    throw std::runtime_error("start position beyond sequence length (" + std::to_string(start) + " > " + std::to_string(limit) + ") for non-circular sequence");
                }

                // The LHS should not overflow as 'spos >= 1' so 'limit - spos + 1' should still be no greater than 'limit'.
                if (limit - spos + 1 < width) {
                    throw std::runtime_error("end position beyond sequence length (" + 
                        std::to_string(start) + " + " + std::to_string(width) + " > " + std::to_string(limit) + 
                        ") for non-circular sequence");
                }
            }
        }

        bool exceeded = false;
        if (start > 0) {
            // 'end_limit - start' is always non-negative as 'end_limit' is the largest value of an int64_t and 'start' is also int64_t.
            exceeded = (end_limit - static_cast<uint64_t>(start) < width);
        } else {
            // 'end_limit - start' will not overflow a uint64_t, because 'end_limit' is the largest value of an int64_t and 'start' as also 'int64_t'.
            exceeded = (end_limit + static_cast<uint64_t>(-start) < width);
        }
        if (exceeded) {
            throw std::runtime_error("end position beyond the range of a 64-bit integer (" + std::to_string(start) + " + " + std::to_string(width) + ")");
        }
    }

    {       
        auto strand_handle = ritsuko::hdf5::open_dataset(ghandle, "strand");
        if (num_ranges != ritsuko::hdf5::get_1d_length(strand_handle, false)) {
            throw std::runtime_error("'strand' and 'sequence' should have the same length");
        }
        if (ritsuko::hdf5::exceeds_integer_limit(strand_handle, 32, true)) {
            throw std::runtime_error("expected 'strand' to have a datatype that fits into a 32-bit signed integer");
        }

        ritsuko::hdf5::Stream1dNumericDataset<int32_t> strand_stream(&strand_handle, num_ranges, options.hdf5_buffer_size);
        for (hsize_t i = 0; i < num_ranges; ++i, strand_stream.next()) {
            auto x = strand_stream.get();
            if (x < -1 || x > 1) {
                throw std::runtime_error("values of 'strand' should be one of 0, -1, or 1 (got " + std::to_string(x) + ")");
            }
        }
    }

    internal_other::validate_mcols(path, "range_annotations", num_ranges, options);
    internal_other::validate_metadata(path, "other_annotations", options);

    internal_string::validate_names(ghandle, "name", num_ranges, options.hdf5_buffer_size);
}

/**
 * @param path Path to a directory containing genomic ranges.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return The number of ranges.
 */
inline size_t height(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    auto handle = ritsuko::hdf5::open_file(path / "ranges.h5");
    auto ghandle = handle.openGroup("genomic_ranges");
    auto dhandle = ghandle.openDataSet("sequence");
    return ritsuko::hdf5::get_1d_length(dhandle, false);
}

}

}

#endif
