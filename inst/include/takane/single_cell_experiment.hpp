#ifndef TAKANE_SINGLE_CELL_EXPERIMENT_HPP
#define TAKANE_SINGLE_CELL_EXPERIMENT_HPP

#include "millijson/millijson.hpp"
#include "byteme/byteme.hpp"

#include "summarized_experiment.hpp"

#include <filesystem>
#include <stdexcept>
#include <unordered_set>
#include <string>

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, const std::string&, const Options& options);
std::vector<size_t> dimensions(const std::filesystem::path&, const std::string&, const Options& options);
/**
 * @endcond
 */

/**
 * @namespace takane::single_cell_experiment
 * @brief Definitions for single cell experiments.
 */
namespace single_cell_experiment {

namespace internal {

inline void validate(const std::filesystem::path& path, const std::string& objname, const Options& options) {
    ::takane::ranged_summarized_experiment::internal::validate(path, "single_cell_experiment", options);
    auto dims = ::takane::summarized_experiment::dimensions(path, options);
    size_t num_cols = dims[1];

    // Check whether we have a valid 'single_cell_experiment.json'.
    auto mpath = path / "single_cell_experiment.json";
    auto parsed = millijson::parse_file(mpath.c_str());
    const millijson::Object* optr;
    try {
        if (parsed->type() != millijson::OBJECT) {
            throw std::runtime_error("expected a top-level object");
        }
        optr = reinterpret_cast<const millijson::Object*>(parsed.get());

        const auto& vstring = internal_summarized_experiment::validate_version_json(optr);
        auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
        if (version.major != 1) {
            throw std::runtime_error("unsupported version string '" + vstring + "'");
        }
    } catch (std::exception& e) {
        throw std::runtime_error("invalid 'single_cell_experiment.json' file; " + std::string(e.what()));
    }

    // Check the reduced dimensions.
    auto rddir = path / "reduced_dimensions";
    if (std::filesystem::exists(rddir)) {
        auto num_rd = internal_summarized_experiment::check_names_json(rddir);

        for (size_t i = 0; i < num_rd; ++i) {
            auto rdname = std::to_string(i);
            auto rdpath = rddir / rdname;
            auto rdtype = read_object_type(rdpath);
            ::takane::validate(rdpath, rdtype, options);

            auto dims = ::takane::dimensions(rdpath, rdtype, options);
            if (dims.size() < 1) {
                throw std::runtime_error("object in 'reduced_dimensions/" + rdname + "' should have at least one dimension");
            }
            if (dims[0] != num_cols) {
                throw std::runtime_error("object in 'reduced_dimensions/" + rdname + "' should have the same number of rows as the columns of its parent '" + objname + "'");
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
            auto aetype = read_object_type(aepath);
            if (!satisfies_interface(aetype, "SUMMARIZED_EXPERIMENT")) {
                throw std::runtime_error("object in 'alternative_experiments/" + aename + "' should satisfy the 'SUMMARIZED_EXPERIMENT' interface");
            }

            ::takane::validate(aepath, aetype, options);
            auto dims = ::takane::dimensions(aepath, aetype, options);
            if (dims[1] != num_cols) {
                throw std::runtime_error("object in 'alternative_experiments/" + aename + "' should have the same number of columns as its parent '" + objname + "'");
            }
        }

        size_t num_dir_obj = internal_other::count_directory_entries(aedir);
        if (num_dir_obj - 1 != num_ae) { // -1 to account for the names.json file itself.
            throw std::runtime_error("more objects than expected inside the 'alternative_experiments' subdirectory");
        }
    }

    // Validating the main experiment name.
    auto mIt = optr->values.find("main_experiment_name");
    if (mIt != optr->values.end()) {
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

/**
 * @param path Path to the directory containing the single cell experiment.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const Options& options) try {
    internal::validate(path, "single_cell_experiment", options);
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate 'single_cell_experiment' object at '" + path.string() + "'; " + std::string(e.what()));
}

}

}

#endif
