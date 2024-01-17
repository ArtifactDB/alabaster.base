#ifndef TAKANE_MULTI_SAMPLE_DATASET_HPP
#define TAKANE_MULTI_SAMPLE_DATASET_HPP

#include <filesystem>
#include <string>
#include <cstdint>
#include <stdexcept>

#include "utils_public.hpp"
#include "utils_other.hpp"
#include "utils_summarized_experiment.hpp"

/**
 * @file multi_sample_dataset.hpp
 * @brief Validation for multi-sample datasets.
 */

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, const ObjectMetadata&, Options&);
size_t height(const std::filesystem::path&, const ObjectMetadata&, Options&);
bool satisfies_interface(const std::string&, const std::string&, const Options&);
/**
 * @endcond
 */

/**
 * @namespace takane::multi_sample_dataset
 * @brief Definitions for multi-sample datasets.
 */
namespace multi_sample_dataset {

/**
 * @param path Path to the directory containing the multi-sample dataset.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    const auto& dmap = internal_json::extract_typed_object_from_metadata(metadata.other, "multi_sample_dataset");

    const std::string& vstring = internal_json::extract_string_from_typed_object(dmap, "version", "multi_sample_dataset");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    // Sample data should exist.
    auto sd_path = path / "sample_data";
    auto sdmeta = read_object_metadata(sd_path);
    if (!satisfies_interface(sdmeta.type, "DATA_FRAME", options)) {
        throw std::runtime_error("object in 'sample_data' should satisfy the 'DATA_FRAME' interface");
    }
    try {
        ::takane::validate(sd_path, sdmeta, options);
    } catch (std::exception& e) {
        throw std::runtime_error("failed to validate 'sample_data'; " + std::string(e.what()));
    }
    size_t num_samples = ::takane::height(sd_path, sdmeta, options);

    // Checking the experiments.
    std::vector<size_t> num_columns;
    auto edir = path / "experiments";
    if (std::filesystem::exists(edir)) {
        size_t num_experiments = internal_summarized_experiment::check_names_json(edir);
        num_columns.reserve(num_experiments);

        for (size_t e = 0; e < num_experiments; ++e) {
            auto ename = std::to_string(e);
            auto epath = edir / ename;
            auto emeta = read_object_metadata(epath);

            if (!satisfies_interface(emeta.type, "SUMMARIZED_EXPERIMENT", options)) {
                throw std::runtime_error("object in 'experiments/" + ename + "' should satisfy the 'SUMMARIZED_EXPERIMENT' interface");
            }

            try {
                ::takane::validate(epath, emeta, options);
            } catch (std::exception& e) {
                throw std::runtime_error("failed to validate 'experiments/" + ename + "'; " + std::string(e.what()));
            }

            auto dims = ::takane::dimensions(epath, emeta, options);
            num_columns.push_back(dims[1]);
        }

        size_t num_dir_obj = internal_other::count_directory_entries(edir);
        if (num_dir_obj - 1 != num_experiments) { // -1 to account for the names.json file itself.
            throw std::runtime_error("more objects than expected inside the 'experiments' subdirectory");
        }
    }

    // Checking the sample map.
    if (num_columns.size() > 0) {
        try {
            auto handle = ritsuko::hdf5::open_file(path / "sample_map.h5");
            auto ghandle = ritsuko::hdf5::open_group(handle, "multi_sample_dataset");

            for (size_t e = 0, end = num_columns.size(); e < end; ++e) {
                auto ename = std::to_string(e);
                auto dhandle = ritsuko::hdf5::open_dataset(ghandle, ename.c_str());
                if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 64, false)) {
                    throw std::runtime_error("'multi_sample_dataset/" + ename + "' should have a datatype that fits into a 64-bit unsigned integer");
                }

                auto len = ritsuko::hdf5::get_1d_length(dhandle.getSpace(), false);
                if (len != num_columns[e]) {
                    throw std::runtime_error("length of 'multi_sample_dataset/" + ename + "' should equal the number of columns of 'experiments/" + ename + "'");
                }

                ritsuko::hdf5::Stream1dNumericDataset<uint64_t> stream(&dhandle, len, options.hdf5_buffer_size);
                for (hsize_t i = 0; i < len; ++i, stream.next()) {
                    auto x = stream.get();
                    if (static_cast<size_t>(x) >= num_samples) {
                        throw std::runtime_error("indices in 'multi_sample_dataset/" + ename + "' should be less than the number of samples");
                    }
                }
            }

            if (num_columns.size() != ghandle.getNumObjs()) {
                throw std::runtime_error("more objects present in the 'multi_sample_dataset' group than expected");
            }
        } catch (std::exception& e) {
            throw std::runtime_error("failed to validate the sample mapping; " + std::string(e.what()));
        }
    }

    internal_other::validate_metadata(path, "other_data", options);
}

}

}

#endif
