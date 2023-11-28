#ifndef TAKANE_SUMMARIZED_EXPERIMENT_HPP
#define TAKANE_SUMMARIZED_EXPERIMENT_HPP

#include "millijson/millijson.hpp"
#include "byteme/byteme.hpp"

#include "utils_public.hpp"
#include "utils_other.hpp"

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
std::vector<size_t> dimensions(const std::filesystem::path&, const std::string&, const Options& options);
/**
 * @endcond
 */

/**
 * @namespace takane::summarized_experiment
 * @brief Definitions for summarized experiments.
 */
namespace summarized_experiment {

/**
 * @cond
 */
namespace internal {

inline void validate(const std::filesystem::path& path, const std::string& objname, const Options& options) {
    auto mpath = path / "summarized_experiment.json";
    auto parsed = millijson::parse_file(mpath.c_str());

    size_t num_rows = 0, num_cols = 0;
    try {
        if (parsed->type() != millijson::OBJECT) {
            throw std::runtime_error("expected a top-level object");
        }
        auto optr = reinterpret_cast<const millijson::Object*>(parsed.get());

        // Validating the version.
        {
            auto vIt = optr->values.find("version");
            if (vIt == optr->values.end()) {
                throw std::runtime_error("expected a 'version' property");
            }
            const auto& ver = vIt->second;
            if (ver->type() != millijson::STRING) {
                throw std::runtime_error("expected 'version' to be a string");
            }

            const auto& vstring = reinterpret_cast<const millijson::String*>(ver.get())->value;
            auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
            if (version.major != 1) {
                throw std::runtime_error("unsupported version string '" + vstring + "'");
            }
        }

        // Validating the dimensions.
        {
            auto dIt = optr->values.find("dimensions");
            if (dIt == optr->values.end()) {
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

    } catch (std::exception& e) {
        throw std::runtime_error("invalid 'summarized_experiment.json' file; " + std::string(e.what()));
    }

    // Checking the assays.
    {
        size_t num_assays = 0;
        auto mpath = path / "assays" / "names.json";
        auto parsed = millijson::parse_file(mpath.c_str());

        try {
            if (parsed->type() != millijson::ARRAY) {
                throw std::runtime_error("expected an array");
            }

            auto aptr = reinterpret_cast<const millijson::Array*>(parsed.get());
            num_assays = aptr->values.size();
            std::unordered_set<std::string> present;
            present.reserve(num_assays);

            for (size_t i = 0; i < num_assays; ++i) {
                auto eptr = aptr->values[i];
                if (eptr->type() != millijson::STRING) {
                    throw std::runtime_error("expected an array of strings");
                }

                auto nptr = reinterpret_cast<const millijson::String*>(eptr.get());
                auto name = nptr->value;
                if (present.find(name) != present.end()) {
                    throw std::runtime_error("detected duplicated assay name '" + name + "'");
                }
                present.insert(std::move(name));
            }

        } catch (std::exception& e) {
            throw std::runtime_error("invalid 'assays/names.json' file; " + std::string(e.what()));
        }

        for (size_t i = 0; i < num_assays; ++i) {
            auto aname = std::to_string(i);
            auto apath = path / "assays" / aname;
            auto atype = read_object_type(apath);
            ::takane::validate(apath, atype, options);

            auto dims = ::takane::dimensions(apath, atype, options);
            if (dims.size() < 2) {
                throw std::runtime_error("object in 'assays/" + aname + "' should have two or more dimensions");
            }
            if (dims[0] != num_rows) {
                throw std::runtime_error("object in 'assays/" + aname + "' should have the same number of rows as its parent '" + objname + "'");
            }
            if (dims[1] != num_cols) {
                throw std::runtime_error("object in 'assays/" + aname + "' should have the same number of columns as its parent '" + objname + "'");
            }
        }

        size_t num_dir_obj = 0;
        for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator(path / "assays")) {
            ++num_dir_obj;
        }
        if (num_dir_obj - 1 != num_assays) { // -1 to account for the names.json file itself.
            throw std::runtime_error("more objects than expected inside the 'assays' subdirectory");
        }
    }

    auto rd_path = path / "row_data";
    if (std::filesystem::exists(rd_path)) {
        auto rd_type = read_object_type(rd_path);
        if (!satisfies_interface(rd_type, "DATA_FRAME")) {
            throw std::runtime_error("object in 'row_data' should satisfy the 'DATA_FRAME' interface");
        }
        ::takane::validate(rd_path, rd_type, options);
        if (::takane::height(rd_path, rd_type, options) != num_rows) {
            throw std::runtime_error("data frame at 'row_data' should have number of rows equal to that of the '" + objname + "'");
        }
    }

    auto cd_path = path / "column_data";
    if (std::filesystem::exists(cd_path)) {
        auto cd_type = read_object_type(cd_path);
        if (!satisfies_interface(cd_type, "DATA_FRAME")) {
            throw std::runtime_error("object in 'column_data' should satisfy the 'DATA_FRAME' interface");
        }
        ::takane::validate(cd_path, cd_type, options);
        if (::takane::height(cd_path, cd_type, options) != num_cols) {
            throw std::runtime_error("data frame at 'column_data' should have number of rows equal to the number of columns of its parent '" + objname + "'");
        }
    }

    internal_other::validate_metadata(path, "other_data", options);
}

}
/**
 * @endcond
 */

/**
 * @param path Path to the directory containing the summarized experiment.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const Options& options) try {
    internal::validate(path, "summarized_experiment", options);
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate 'summarized_experiment' object at '" + path.string() + "'; " + std::string(e.what()));
}

/**
 * @param path Path to a directory containing a summarized experiment.
 * @param options Validation options, mostly for input performance.
 * @return Number of rows in the summarized experiment.
 */
inline size_t height(const std::filesystem::path& path, [[maybe_unused]] const Options& options) {
    auto mpath = path / "summarized_experiment.json";
    auto parsed = millijson::parse_file(mpath.c_str());

    // Assume it's all valid, so we go sraight for the kill.
    auto optr = reinterpret_cast<const millijson::Object*>(parsed.get());
    auto dIt = optr->values.find("dimensions");
    const auto& dims = dIt->second;
    auto dptr = reinterpret_cast<const millijson::Array*>(dims.get());
    return reinterpret_cast<const millijson::Number*>(dptr->values[0].get())->value;
}

/**
 * @param path Path to a directory containing a summarized experiment.
 * @param options Validation options, mostly for input performance.
 * @return A vector of length 2 containing the dimensions of the summarized experiment.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path, [[maybe_unused]] const Options& options) {
    auto mpath = path / "summarized_experiment.json";
    auto parsed = millijson::parse_file(mpath.c_str());

    // Assume it's all valid, so we go sraight for the kill.
    auto optr = reinterpret_cast<const millijson::Object*>(parsed.get());
    auto dIt = optr->values.find("dimensions");
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
