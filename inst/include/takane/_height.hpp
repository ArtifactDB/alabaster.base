#ifndef TAKANE_HEIGHT_HPP
#define TAKANE_HEIGHT_HPP

#include <functional>
#include <string>
#include <stdexcept>
#include <filesystem>

#include "utils_public.hpp"
#include "atomic_vector.hpp"
#include "string_factor.hpp"
#include "simple_list.hpp"
#include "data_frame.hpp"
#include "data_frame_factor.hpp"
#include "genomic_ranges.hpp"
#include "atomic_vector_list.hpp"
#include "data_frame_list.hpp"
#include "genomic_ranges_list.hpp"

/**
 * @file _height.hpp
 * @brief Dispatch to functions for the object's height.
 */

namespace takane {

/**
 * @cond
 */
namespace internal_height {

inline auto default_registry() {
    std::unordered_map<std::string, std::function<size_t(const std::filesystem::path&, const Options&)> > registry;
    registry["atomic_vector"] = [](const std::filesystem::path& p, const Options& o) -> size_t { return atomic_vector::height(p, o); };
    registry["string_factor"] = [](const std::filesystem::path& p, const Options& o) -> size_t { return string_factor::height(p, o); };
    registry["simple_list"] = [](const std::filesystem::path& p, const Options& o) -> size_t { return simple_list::height(p, o); };
    registry["data_frame"] = [](const std::filesystem::path& p, const Options& o) -> size_t { return data_frame::height(p, o); };
    registry["data_frame_factor"] = [](const std::filesystem::path& p, const Options& o) -> size_t { return data_frame_factor::height(p, o); };
    registry["genomic_ranges"] = [](const std::filesystem::path& p, const Options& o) -> size_t { return genomic_ranges::height(p, o); };
    registry["atomic_vector_list"] = [](const std::filesystem::path& p, const Options& o) -> size_t { return atomic_vector_list::height(p, o); };
    registry["data_frame_list"] = [](const std::filesystem::path& p, const Options& o) -> size_t { return data_frame_list::height(p, o); };
    registry["genomic_ranges_list"] = [](const std::filesystem::path& p, const Options& o) -> size_t { return genomic_ranges_list::height(p, o); };
    return registry;
} 

}
/**
 * @endcond
 */

/**
 * Registry of functions to be used by `height()`.
 * Applications can extend **takane** by adding new height functions for custom object types.
 */
inline std::unordered_map<std::string, std::function<size_t(const std::filesystem::path&, const Options&)> > height_registry = internal_height::default_registry();

/**
 * Get the height of an object in a subdirectory, based on the supplied object type.
 * This searches the `height_registry` to find a height function for the given type.
 *
 * `height()` is used to check the shape of objects stored in vertical containers, e.g., columns of a `data_frame`.
 * For vectors or other 1-dimensional objects, the height is usually just the length of the object (for some object-specific definition of "length").
 * For higher-dimensional objects, the height is usually the extent of the first dimension.
 *
 * @param path Path to a directory representing an object.
 * @param type Type of the object, typically determined from its `OBJECT` file.
 * @param options Validation options, mostly for input performance.
 *
 * @return The object's height.
 */
inline size_t height(const std::filesystem::path& path, const std::string& type, const Options& options) {
    if (!std::filesystem::exists(path) || std::filesystem::status(path).type() != std::filesystem::file_type::directory) {
        throw std::runtime_error("expected '" + path.string() + "' to be a directory");
    }

    auto vrIt = height_registry.find(type);
    if (vrIt == height_registry.end()) {
        throw std::runtime_error("no registered 'height' function for object type '" + type + "' at '" + path.string() + "'");
    }

    return (vrIt->second)(path, options);
}

/**
 * Get the height of an object in a subdirectory, using its `OBJECT` file to automatically determine the type.
 *
 * @param path Path to a directory containing an object.
 * @param options Validation options, mostly for input performance.
 * @return The object's height.
 */
inline size_t height(const std::filesystem::path& path, const Options& options) {
    return height(path, read_object_type(path), options);
}

/**
 * Overload of `height()` with default options.
 *
 * @param path Path to a directory containing an object.
 * @return The object's height.
 */
inline size_t height(const std::filesystem::path& path) {
    return height(path, Options());
}

}

#endif
