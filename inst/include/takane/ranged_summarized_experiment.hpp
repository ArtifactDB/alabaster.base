#ifndef TAKANE_RANGED_SUMMARIZED_EXPERIMENT_HPP
#define TAKANE_RANGED_SUMMARIZED_EXPERIMENT_HPP

#include "millijson/millijson.hpp"
#include "byteme/byteme.hpp"

#include "summarized_experiment.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

/**
 * @file ranged_summarized_experiment.hpp
 * @brief Validation for ranged summarized experiments.
 */

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, const ObjectMetadata&, Options& options);
size_t height(const std::filesystem::path&, const ObjectMetadata&, Options& options);
bool derived_from(const std::string&, const std::string&, const Options&);
/**
 * @endcond
 */

/**
 * @namespace takane::ranged_summarized_experiment
 * @brief Definitions for ranged summarized experiments.
 */
namespace ranged_summarized_experiment {

/**
 * @param path Path to the directory containing the ranged summarized experiment.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    ::takane::summarized_experiment::validate(path, metadata, options);

    const auto& rsemap = internal_json::extract_typed_object_from_metadata(metadata.other, "ranged_summarized_experiment");

    const std::string& vstring = internal_json::extract_string_from_typed_object(rsemap, "version", "ranged_summarized_experiment");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    auto rangedir = path / "row_ranges";
    if (std::filesystem::exists(rangedir)) {
        auto rangemeta = read_object_metadata(rangedir);
        if (!derived_from(rangemeta.type, "genomic_ranges", options) && !derived_from(rangemeta.type, "genomic_ranges_list", options)) {
            throw std::runtime_error("object in 'row_ranges' must be a 'genomic_ranges' or 'genomic_ranges_list'");
        }

        ::takane::validate(rangedir, rangemeta, options);

        auto num_row = ::takane::summarized_experiment::height(path, metadata, options);
        if (::takane::height(rangedir, rangemeta, options) != num_row) {
            throw std::runtime_error("object in 'row_ranges' must have length equal to the number of rows of its parent '" + metadata.type + "'");
        }
    }
}

}

}

#endif
