#ifndef TAKANE_SEQUENCE_INFORMATION_HPP
#define TAKANE_SEQUENCE_INFORMATION_HPP

#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include <filesystem>
#include <stdexcept>
#include <unordered_set>
#include <string>

#include "utils_public.hpp"
#include "utils_json.hpp"

/**
 * @file sequence_information.hpp
 * @brief Validation for sequence information.
 */

namespace takane {

/**
 * @namespace takane::sequence_information
 * @brief Definitions for sequence information objects.
 */
namespace sequence_information {

/**
 * @param path Path to the directory containing the data frame.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    auto vstring = internal_json::extract_version_for_type(metadata.other, "sequence_information");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    auto handle = ritsuko::hdf5::open_file(path / "info.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "sequence_information");

    size_t nseq = 0;
    {
        auto nhandle = ritsuko::hdf5::open_dataset(ghandle, "name");
        if (!ritsuko::hdf5::is_utf8_string(nhandle)) {
            throw std::runtime_error("expected 'name' to have a datatype that can be represented by a UTF-8 encoded string");
        }

        nseq = ritsuko::hdf5::get_1d_length(nhandle.getSpace(), false);
        std::unordered_set<std::string> collected;
        ritsuko::hdf5::Stream1dStringDataset stream(&nhandle, nseq, options.hdf5_buffer_size);
        for (size_t s = 0; s < nseq; ++s, stream.next()) {
            auto x = stream.steal();
            if (collected.find(x) != collected.end()) {
                throw std::runtime_error("detected duplicated sequence name '" + x + "'");
            }
            collected.insert(std::move(x));
        }
    }

    const char* missing_attr_name = "missing-value-placeholder";

    {
        auto lhandle = ritsuko::hdf5::open_dataset(ghandle, "length");
        if (ritsuko::hdf5::exceeds_integer_limit(lhandle, 64, false)) {
            throw std::runtime_error("expected a datatype for 'length' that fits in a 64-bit unsigned integer");
        }
        if (ritsuko::hdf5::get_1d_length(lhandle.getSpace(), false) != nseq) {
            throw std::runtime_error("expected lengths of 'length' and 'name' to be equal");
        }
        if (lhandle.attrExists(missing_attr_name)) {
            auto ahandle = lhandle.openAttribute(missing_attr_name);
            ritsuko::hdf5::check_missing_placeholder_attribute(lhandle, ahandle);
        }
    }

    {
        auto chandle = ritsuko::hdf5::open_dataset(ghandle, "circular");
        if (ritsuko::hdf5::exceeds_integer_limit(chandle, 32, true)) {
            throw std::runtime_error("expected a datatype for 'circular' that fits in a 32-bit signed integer");
        }
        if (ritsuko::hdf5::get_1d_length(chandle.getSpace(), false) != nseq) {
            throw std::runtime_error("expected lengths of 'length' and 'circular' to be equal");
        }
        if (chandle.attrExists(missing_attr_name)) {
            auto ahandle = chandle.openAttribute(missing_attr_name);
            ritsuko::hdf5::check_missing_placeholder_attribute(chandle, ahandle);
        }
    }

    {
        auto gnhandle = ritsuko::hdf5::open_dataset(ghandle, "genome");
        if (!ritsuko::hdf5::is_utf8_string(gnhandle)) {
            throw std::runtime_error("expected 'genome' to have a datatype that can be represented by a UTF-8 encoded string");
        }
        if (ritsuko::hdf5::get_1d_length(gnhandle.getSpace(), false) != nseq) {
            throw std::runtime_error("expected lengths of 'length' and 'genome' to be equal");
        }
        if (gnhandle.attrExists(missing_attr_name)) {
            auto ahandle = gnhandle.openAttribute(missing_attr_name);
            ritsuko::hdf5::check_missing_placeholder_attribute(gnhandle, ahandle);
        }
    }
}

}

}

#endif
