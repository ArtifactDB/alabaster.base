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
 * @cond
 */
namespace internal_dimensions {

inline auto default_registry() {
    std::unordered_map<std::string, std::function<std::vector<size_t>(const std::filesystem::path&, const Options&)> > registry;
    registry["data_frame"] = [](const std::filesystem::path& p, const Options& o) -> std::vector<size_t> { return data_frame::dimensions(p, o); };
    registry["dense_array"] = [](const std::filesystem::path& p, const Options& o) -> std::vector<size_t> { return dense_array::dimensions(p, o); };
    registry["compressed_sparse_matrix"] = [](const std::filesystem::path& p, const Options& o) -> std::vector<size_t> { return compressed_sparse_matrix::dimensions(p, o); };

    // Subclasses of SE, so we just re-use the SE methods here.
    registry["summarized_experiment"] = [](const std::filesystem::path& p, const Options& o) -> std::vector<size_t> { return summarized_experiment::dimensions(p, o); };
    registry["ranged_summarized_experiment"] = [](const std::filesystem::path& p, const Options& o) -> std::vector<size_t> { return summarized_experiment::dimensions(p, o); };
    registry["single_cell_experiment"] = [](const std::filesystem::path& p, const Options& o) -> std::vector<size_t> { return summarized_experiment::dimensions(p, o); };

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
inline std::unordered_map<std::string, std::function<std::vector<size_t>(const std::filesystem::path&, const Options&)> > dimensions_registry = internal_dimensions::default_registry();

/**
 * Get the dimensions of a multi-dimensional object in a subdirectory, based on the supplied object type.
 * This searches the `dimensions_registry` to find a dimension function for the given type.
 *
 * @param path Path to a directory representing an object.
 * @param type Type of the object, typically determined from its `OBJECT` file.
 * @param options Validation options, mostly for input performance.
 *
 * @return Vector containing the object's dimensions.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path, const std::string& type, const Options& options) {
    if (!std::filesystem::exists(path) || std::filesystem::status(path).type() != std::filesystem::file_type::directory) {
        throw std::runtime_error("expected '" + path.string() + "' to be a directory");
    }

    auto vrIt = dimensions_registry.find(type);
    if (vrIt == dimensions_registry.end()) {
        throw std::runtime_error("no registered 'dimensions' function for object type '" + type + "' at '" + path.string() + "'");
    }

    return (vrIt->second)(path, options);
}

/**
 * Get the dimensions of an object in a subdirectory, using its `OBJECT` file to automatically determine the type.
 *
 * @param path Path to a directory containing an object.
 * @param options Validation options, mostly for input performance.
 * @return The object's dimensions.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path, const Options& options) {
    return dimensions(path, read_object_type(path), options);
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
