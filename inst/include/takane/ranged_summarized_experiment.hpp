#ifndef TAKANE_RANGED_SUMMARIZED_EXPERIMENT_HPP
#define TAKANE_RANGED_SUMMARIZED_EXPERIMENT_HPP

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
size_t height(const std::filesystem::path&, const std::string&, const Options& options);
/**
 * @endcond
 */

/**
 * @namespace takane::ranged_summarized_experiment
 * @brief Definitions for ranged summarized experiments.
 */
namespace ranged_summarized_experiment {

namespace internal {

inline void validate(const std::filesystem::path& path, const std::string& objname, const Options& options) {
    ::takane::summarized_experiment::internal::validate(path, "ranged_summarized_experiment", options);
    auto num_row = ::takane::summarized_experiment::height(path, options);

    auto rangedir = path / "row_ranges";
    auto rangetype = read_object_type(rangedir);
    if (rangetype != "genomic_ranges" && rangetype != "genomic_ranges_list") {
        throw std::runtime_error("object in 'row_ranges' must be a 'genomic_ranges' or 'genomic_ranges_list'");
    }

    ::takane::validate(rangedir, rangetype, options);
    if (::takane::height(rangedir, options) != num_row) {
        throw std::runtime_error("object in 'row_ranges' must have length equal to the number of rows of its parent '" + objname + "'");
    }
}

}

/**
 * @param path Path to the directory containing the ranged summarized experiment.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const Options& options) try {
    internal::validate(path, "ranged_summarized_experiment", options);
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate 'ranged_summarized_experiment' object at '" + path.string() + "'; " + std::string(e.what()));
}

}

}

#endif
