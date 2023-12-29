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

/**
 * @file _dimensions.hpp
 * @brief Dispatch to functions for the object's dimensions.
 */

namespace takane {

/**
 * Class to map object types to `dimensions()` functions.
 */
typedef std::unordered_map<std::string, std::function<std::vector<size_t>(const std::filesystem::path&, const ObjectMetadata&, const Options&)> > DimensionsRegistry;

/**
 * @cond
 */
namespace internal_dimensions {

inline DimensionsRegistry default_registry() {
    DimensionsRegistry registry;

    registry["data_frame"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) -> std::vector<size_t> { return data_frame::dimensions(p, m, o); };
    registry["dense_array"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) -> std::vector<size_t> { return dense_array::dimensions(p, m, o); };
    registry["compressed_sparse_matrix"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) -> std::vector<size_t> { return compressed_sparse_matrix::dimensions(p, m, o); };

    // Subclasses of SE, so we just re-use the SE methods here.
    registry["summarized_experiment"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) -> std::vector<size_t> { return summarized_experiment::dimensions(p, m, o); };
    registry["ranged_summarized_experiment"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) -> std::vector<size_t> { return summarized_experiment::dimensions(p, m, o); };
    registry["single_cell_experiment"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) -> std::vector<size_t> { return summarized_experiment::dimensions(p, m, o); };
    registry["spatial_experiment"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) -> std::vector<size_t> { return summarized_experiment::dimensions(p, m, o); };

    return registry;
} 

}
/**
 * @endcond
 */

/**
 * Registry of functions to be used by `dimensions()`.
 * Applications can extend **takane** by adding new dimension functions for custom object types.
 */
inline DimensionsRegistry dimensions_registry = internal_dimensions::default_registry();

/**
 * Get the dimensions of a multi-dimensional object in a subdirectory, based on the supplied object type.
 * This searches the `dimensions_registry` to find a dimension function for the given type.
 *
 * @param path Path to a directory representing an object.
 * @param metadata Metadata for the object, typically determined from its `OBJECT` file.
 * @param options Validation options, mostly for input performance.
 *
 * @return Vector containing the object's dimensions.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path, const ObjectMetadata& metadata, const Options& options) {
    if (!std::filesystem::exists(path) || std::filesystem::status(path).type() != std::filesystem::file_type::directory) {
        throw std::runtime_error("expected '" + path.string() + "' to be a directory");
    }

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
 * @param options Validation options, mostly for input performance.
 * @return The object's dimensions.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path, const Options& options) {
    return dimensions(path, read_object_metadata(path), options);
}

/**
 * Overload of `dimensions()` with default options.
 *
 * @param path Path to a directory containing an object.
 * @return The object's dimensions.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path) {
    return dimensions(path, Options());
}

}

#endif
