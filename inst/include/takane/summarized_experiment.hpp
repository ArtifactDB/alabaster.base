#ifndef TAKANE_SUMMARIZED_EXPERIMENT_HPP
#define TAKANE_SUMMARIZED_EXPERIMENT_HPP

#include "millijson/millijson.hpp"
#include "byteme/byteme.hpp"

#include "utils_public.hpp"
#include "utils_other.hpp"
#include "utils_summarized_experiment.hpp"

#include <filesystem>
#include <stdexcept>
#include <unordered_set>
#include <string>

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, const ObjectMetadata&, const Options& options);
size_t height(const std::filesystem::path&, const ObjectMetadata&, const Options& options);
std::vector<size_t> dimensions(const std::filesystem::path&, const ObjectMetadata&, const Options& options);
/**
 * @endcond
 */

/**
 * @namespace takane::summarized_experiment
 * @brief Definitions for summarized experiments.
 */
namespace summarized_experiment {

/**
 * @param path Path to the directory containing the summarized experiment.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, const Options& options) {
    const auto& semap = internal_json::extract_typed_object_from_metadata(metadata.other, "summarized_experiment");

    const std::string& vstring = internal_json::extract_string_from_typed_object(semap, "version", "summarized_experiment");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    // Validating the dimensions.
    size_t num_rows = 0, num_cols = 0;
    {
        auto dIt = semap.find("dimensions");
        if (dIt == semap.end()) {
            throw std::runtime_error("expected a 'dimensions' property");
        }
        const auto& dims = dIt->second;
        if (dims->type() != millijson::ARRAY) {
            throw std::runtime_error("expected 'dimensions' to be an array");
        }

        auto dptr = reinterpret_cast<const millijson::Array*>(dims.get());
        if (dptr->values.size() != 2) {
            throw std::runtime_error("expected 'dimensions' to be an array of length 2");
        }

        size_t counter = 0;
        for (const auto& x : dptr->values) {
            if (x->type() != millijson::NUMBER) {
                throw std::runtime_error("expected 'dimensions' to be an array of numbers");
            }
            auto val = reinterpret_cast<const millijson::Number*>(x.get())->value;
            if (val < 0 || std::floor(val) != val) {
                throw std::runtime_error("expected 'dimensions' to contain non-negative integers");
            }
            if (counter == 0) {
                num_rows = val;
            } else {
                num_cols = val;
            }
            ++counter;
        }
    }

    // Checking the assays. The directory is also allowed to not exist, 
    // in which case we have no assays.
    auto adir = path / "assays";
    if (std::filesystem::exists(adir)) {
        size_t num_assays = internal_summarized_experiment::check_names_json(adir);
        for (size_t i = 0; i < num_assays; ++i) {
            auto aname = std::to_string(i);
            auto apath = adir / aname;
            auto ameta = read_object_metadata(apath);
            ::takane::validate(apath, ameta, options);

            auto dims = ::takane::dimensions(apath, ameta, options);
            if (dims.size() < 2) {
                throw std::runtime_error("object in 'assays/" + aname + "' should have two or more dimensions");
            }
            if (dims[0] != num_rows) {
                throw std::runtime_error("object in 'assays/" + aname + "' should have the same number of rows as its parent '" + metadata.type + "'");
            }
            if (dims[1] != num_cols) {
                throw std::runtime_error("object in 'assays/" + aname + "' should have the same number of columns as its parent '" + metadata.type + "'");
            }
        }

        size_t num_dir_obj = internal_other::count_directory_entries(adir);
        if (num_dir_obj - 1 != num_assays) { // -1 to account for the names.json file itself.
            throw std::runtime_error("more objects than expected inside the 'assays' subdirectory");
        }
    }

    auto rd_path = path / "row_data";
    if (std::filesystem::exists(rd_path)) {
        auto rdmeta = read_object_metadata(rd_path);
        if (!satisfies_interface(rdmeta.type, "DATA_FRAME")) {
            throw std::runtime_error("object in 'row_data' should satisfy the 'DATA_FRAME' interface");
        }
        ::takane::validate(rd_path, rdmeta, options);
        if (::takane::height(rd_path, rdmeta, options) != num_rows) {
            throw std::runtime_error("data frame at 'row_data' should have number of rows equal to that of the '" + metadata.type + "'");
        }
    }

    auto cd_path = path / "column_data";
    if (std::filesystem::exists(cd_path)) {
        auto cdmeta = read_object_metadata(cd_path);
        if (!satisfies_interface(cdmeta.type, "DATA_FRAME")) {
            throw std::runtime_error("object in 'column_data' should satisfy the 'DATA_FRAME' interface");
        }
        ::takane::validate(cd_path, cdmeta, options);
        if (::takane::height(cd_path, cdmeta, options) != num_cols) {
            throw std::runtime_error("data frame at 'column_data' should have number of rows equal to the number of columns of its parent '" + metadata.type + "'");
        }
    }

    internal_other::validate_metadata(path, "other_data", options);
}

/**
 * @param path Path to a directory containing a summarized experiment.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, mostly for input performance.
 * @return Number of rows in the summarized experiment.
 */
inline size_t height([[maybe_unused]] const std::filesystem::path& path, const ObjectMetadata& metadata, [[maybe_unused]] const Options& options) {
    // Assume it's all valid, so we go straight for the kill.
    const auto& semap = internal_json::extract_object(metadata.other, "summarized_experiment");
    auto dIt = semap.find("dimensions");
    const auto& dims = dIt->second;
    auto dptr = reinterpret_cast<const millijson::Array*>(dims.get());
    return reinterpret_cast<const millijson::Number*>(dptr->values[0].get())->value;
}

/**
 * @param path Path to a directory containing a summarized experiment.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, mostly for input performance.
 * @return A vector of length 2 containing the dimensions of the summarized experiment.
 */
inline std::vector<size_t> dimensions([[maybe_unused]] const std::filesystem::path& path, const ObjectMetadata& metadata, [[maybe_unused]] const Options& options) {
    // Assume it's all valid, so we go straight for the kill.
    const auto& semap = internal_json::extract_object(metadata.other, "summarized_experiment");
    auto dIt = semap.find("dimensions");
    const auto& dims = dIt->second;
    auto dptr = reinterpret_cast<const millijson::Array*>(dims.get());

    std::vector<size_t> output(2);
    output[0] = reinterpret_cast<const millijson::Number*>(dptr->values[0].get())->value;
    output[1] = reinterpret_cast<const millijson::Number*>(dptr->values[1].get())->value;
    return output;
}

}

}

#endif
