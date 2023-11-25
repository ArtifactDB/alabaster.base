#ifndef TAKANE_SEQUENCE_INFORMATION_HPP
#define TAKANE_SEQUENCE_INFORMATION_HPP

#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include <filesystem>
#include <stdexcept>
#include <unordered_set>
#include <string>

#include "utils_public.hpp"

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
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const Options& options) try {
    auto handle = ritsuko::hdf5::open_file(path / "info.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "sequence_information");

    auto vstring = ritsuko::hdf5::open_and_load_scalar_string_attribute(ghandle, "version");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    size_t nseq = 0;
    {
        auto nhandle = ritsuko::hdf5::open_dataset(ghandle, "name");
        if (nhandle.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("expected a string datatype class for 'name'");
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
        if (gnhandle.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("expected a string datatype class for 'genome'");
        }
        if (ritsuko::hdf5::get_1d_length(gnhandle.getSpace(), false) != nseq) {
            throw std::runtime_error("expected lengths of 'length' and 'genome' to be equal");
        }
        if (gnhandle.attrExists(missing_attr_name)) {
            auto ahandle = gnhandle.openAttribute(missing_attr_name);
            ritsuko::hdf5::check_missing_placeholder_attribute(gnhandle, ahandle);
        }
    }
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate 'sequence_information' object at '" + path.string() + "'; " + std::string(e.what()));
}

}

}

#endif
