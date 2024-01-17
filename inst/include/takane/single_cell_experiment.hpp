#ifndef TAKANE_SINGLE_CELL_EXPERIMENT_HPP
#define TAKANE_SINGLE_CELL_EXPERIMENT_HPP

#include "millijson/millijson.hpp"
#include "byteme/byteme.hpp"

#include "summarized_experiment.hpp"
#include "ranged_summarized_experiment.hpp"

#include <filesystem>
#include <stdexcept>
#include <unordered_set>
#include <string>

/**
 * @file single_cell_experiment.hpp
 * @brief Validation for single cell experiments.
 */

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, const ObjectMetadata&, Options& options);
std::vector<size_t> dimensions(const std::filesystem::path&, const ObjectMetadata&, Options& options);
bool satisfies_interface(const std::string&, const std::string&, const Options&);
/**
 * @endcond
 */

/**
 * @namespace takane::single_cell_experiment
 * @brief Definitions for single cell experiments.
 */
namespace single_cell_experiment {

/**
 * @param path Path to the directory containing the single cell experiment.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    ::takane::ranged_summarized_experiment::validate(path, metadata, options);

    auto sedims = ::takane::summarized_experiment::dimensions(path, metadata, options);
    size_t num_cols = sedims[1];

    const auto& scemap = internal_json::extract_typed_object_from_metadata(metadata.other, "single_cell_experiment");

    const std::string& vstring = internal_json::extract_string_from_typed_object(scemap, "version", "single_cell_experiment");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    // Check the reduced dimensions.
    auto rddir = path / "reduced_dimensions";
    if (std::filesystem::exists(rddir)) {
        auto num_rd = internal_summarized_experiment::check_names_json(rddir);

        for (size_t i = 0; i < num_rd; ++i) {
            auto rdname = std::to_string(i);
            auto rdpath = rddir / rdname;
            auto rdmeta = read_object_metadata(rdpath);
            ::takane::validate(rdpath, rdmeta, options);

            auto dims = ::takane::dimensions(rdpath, rdmeta, options);
            if (dims.size() < 1) {
                throw std::runtime_error("object in 'reduced_dimensions/" + rdname + "' should have at least one dimension");
            }
            if (dims[0] != num_cols) {
                throw std::runtime_error("object in 'reduced_dimensions/" + rdname + "' should have the same number of rows as the columns of its parent '" + metadata.type + "'");
            }
        }

        size_t num_dir_obj = internal_other::count_directory_entries(rddir);
        if (num_dir_obj - 1 != num_rd) { // -1 to account for the names.json file itself.
            throw std::runtime_error("more objects than expected inside the 'reduced_dimensions' subdirectory");
        }
    }

    // Check the alternative experiments.
    auto aedir = path / "alternative_experiments";
    std::unordered_set<std::string> alt_names;
    if (std::filesystem::exists(aedir)) {
        internal_summarized_experiment::check_names_json(aedir, alt_names);
        size_t num_ae = alt_names.size();

        for (size_t i = 0; i < num_ae; ++i) {
            auto aename = std::to_string(i);
            auto aepath = aedir / aename;
            auto aemeta = read_object_metadata(aepath);
            if (!satisfies_interface(aemeta.type, "SUMMARIZED_EXPERIMENT", options)) {
                throw std::runtime_error("object in 'alternative_experiments/" + aename + "' should satisfy the 'SUMMARIZED_EXPERIMENT' interface");
            }

            ::takane::validate(aepath, aemeta, options);
            auto dims = ::takane::dimensions(aepath, aemeta, options);
            if (dims[1] != num_cols) {
                throw std::runtime_error("object in 'alternative_experiments/" + aename + "' should have the same number of columns as its parent '" + metadata.type + "'");
            }
        }

        size_t num_dir_obj = internal_other::count_directory_entries(aedir);
        if (num_dir_obj - 1 != num_ae) { // -1 to account for the names.json file itself.
            throw std::runtime_error("more objects than expected inside the 'alternative_experiments' subdirectory");
        }
    }

    // Validating the main experiment name.
    auto mIt = scemap.find("main_experiment_name");
    if (mIt != scemap.end()) {
        const auto& ver = mIt->second;
        if (ver->type() != millijson::STRING) {
            throw std::runtime_error("expected 'main_experiment_name' to be a string");
        }
        const auto& mname = reinterpret_cast<const millijson::String*>(ver.get())->value;
        if (mname.empty()) {
            throw std::runtime_error("expected 'main_experiment_name' to be a non-empty string");
        }
        if (alt_names.find(mname) != alt_names.end()) {
            throw std::runtime_error("expected 'main_experiment_name' to not overlap with 'alternative_experiment' names (found '" + mname + "')");
        }
    }
}

}

}

#endif
