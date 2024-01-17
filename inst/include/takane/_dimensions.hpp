#ifndef TAKANE_DIMENSIONS_HPP
#define TAKANE_DIMENSIONS_HPP

#include <functional>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <vector>

#include "data_frame.hpp"
#include "dense_array.hpp"
#include "compressed_sparse_matrix.hpp"
#include "summarized_experiment.hpp"
#include "bumpy_atomic_array.hpp"
#include "bumpy_data_frame_array.hpp"
#include "vcf_experiment.hpp"

/**
 * @file _dimensions.hpp
 * @brief Dispatch to functions for the object's dimensions.
 */

namespace takane {

/**
 * @cond
 */
namespace internal_dimensions {

inline auto default_registry() {
    std::unordered_map<std::string, std::function<std::vector<size_t>(const std::filesystem::path&, const ObjectMetadata&, Options& os)> > registry;
    typedef std::vector<size_t> Dims;

    registry["data_frame"] = [](const std::filesystem::path& p, const ObjectMetadata& m, Options& o) -> Dims { return data_frame::dimensions(p, m, o); };
    registry["dense_array"] = [](const std::filesystem::path& p, const ObjectMetadata& m, Options& o) -> Dims { return dense_array::dimensions(p, m, o); };
    registry["compressed_sparse_matrix"] = [](const std::filesystem::path& p, const ObjectMetadata& m, Options& o) -> Dims { return compressed_sparse_matrix::dimensions(p, m, o); };

    // Subclasses of SE, so we just re-use the SE methods here.
    registry["summarized_experiment"] = [](const std::filesystem::path& p, const ObjectMetadata& m, Options& o) -> Dims { return summarized_experiment::dimensions(p, m, o); };
    registry["ranged_summarized_experiment"] = [](const std::filesystem::path& p, const ObjectMetadata& m, Options& o) -> Dims { return summarized_experiment::dimensions(p, m, o); };
    registry["single_cell_experiment"] = [](const std::filesystem::path& p, const ObjectMetadata& m, Options& o) -> Dims { return summarized_experiment::dimensions(p, m, o); };
    registry["spatial_experiment"] = [](const std::filesystem::path& p, const ObjectMetadata& m, Options& o) -> Dims { return summarized_experiment::dimensions(p, m, o); };

    registry["bumpy_atomic_array"] = [](const std::filesystem::path& p, const ObjectMetadata& m, Options& o) -> Dims { return bumpy_atomic_array::dimensions(p, m, o); };
    registry["bumpy_data_frame_array"] = [](const std::filesystem::path& p, const ObjectMetadata& m, Options& o) -> Dims { return bumpy_data_frame_array::dimensions(p, m, o); };
    registry["vcf_experiment"] = [](const std::filesystem::path& p, const ObjectMetadata& m, Options& o) -> Dims { return vcf_experiment::dimensions(p, m, o); };
    registry["delayed_array"] = [](const std::filesystem::path& p, const ObjectMetadata& m, Options& o) -> Dims { return delayed_array::dimensions(p, m, o); };

    return registry;
} 

}
/**
 * @endcond
 */

/**
 * Get the dimensions of a multi-dimensional object in a subdirectory, based on the supplied object type.
 *
 * Applications can supply custom dimension functions for a given type via `Options::custom_dimensions`.
 * If available, the supplied custom function will be used instead of the default.
 *
 * @param path Path to a directory representing an object.
 * @param metadata Metadata for the object, typically determined from its `OBJECT` file.
 * @param options Validation options.
 *
 * @return Vector containing the object's dimensions.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    auto cIt = options.custom_dimensions.find(metadata.type);
    if (cIt != options.custom_dimensions.end()) {
        return (cIt->second)(path, metadata, options);
    }

    static const auto dimensions_registry = internal_dimensions::default_registry();
    auto vrIt = dimensions_registry.find(metadata.type);
    if (vrIt == dimensions_registry.end()) {
        throw std::runtime_error("no registered 'dimensions' function for object type '" + metadata.type + "' at '" + path.string() + "'");
    }

    return (vrIt->second)(path, metadata, options);
}

/**
 * Get the dimensions of an object in a subdirectory, using its `OBJECT` file to automatically determine the type.
 *
 * @param path Path to a directory containing an object.
 * @param options Validation options.
 * @return The object's dimensions.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path, Options& options) {
    return dimensions(path, read_object_metadata(path), options);
}

/**
 * Overload of `dimensions()` with default settings.
 *
 * @param path Path to a directory containing an object.
 * @return The object's dimensions.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path) {
    Options options;
    return dimensions(path, options);
}

}

#endif
