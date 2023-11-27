#ifndef TAKANE_VALIDATE_HPP
#define TAKANE_VALIDATE_HPP

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
#include "sequence_information.hpp"
#include "genomic_ranges.hpp"
#include "atomic_vector_list.hpp"
#include "data_frame_list.hpp"
#include "genomic_ranges_list.hpp"
#include "dense_array.hpp"
#include "compressed_sparse_matrix.hpp"

/**
 * @file _validate.hpp
 * @brief Validation dispatch function.
 */

namespace takane {

/**
 * @cond
 */
namespace internal_validate {

inline auto default_registry() {
    std::unordered_map<std::string, std::function<void(const std::filesystem::path&, const Options&)> > registry;
    registry["atomic_vector"] = [](const std::filesystem::path& p, const Options& o) { atomic_vector::validate(p, o); };
    registry["string_factor"] = [](const std::filesystem::path& p, const Options& o) { string_factor::validate(p, o); };
    registry["simple_list"] = [](const std::filesystem::path& p, const Options& o) { simple_list::validate(p, o); };
    registry["data_frame"] = [](const std::filesystem::path& p, const Options& o) { data_frame::validate(p, o); };
    registry["data_frame_factor"] = [](const std::filesystem::path& p, const Options& o) { data_frame_factor::validate(p, o); };
    registry["sequence_information"] = [](const std::filesystem::path& p, const Options& o) { sequence_information::validate(p, o); };
    registry["genomic_ranges"] = [](const std::filesystem::path& p, const Options& o) { genomic_ranges::validate(p, o); };
    registry["atomic_vector_list"] = [](const std::filesystem::path& p, const Options& o) { atomic_vector_list::validate(p, o); };
    registry["data_frame_list"] = [](const std::filesystem::path& p, const Options& o) { data_frame_list::validate(p, o); };
    registry["genomic_ranges_list"] = [](const std::filesystem::path& p, const Options& o) { genomic_ranges_list::validate(p, o); };
    registry["dense_array"] = [](const std::filesystem::path& p, const Options& o) { dense_array::validate(p, o); };
    registry["compressed_sparse_matrix"] = [](const std::filesystem::path& p, const Options& o) { compressed_sparse_matrix::validate(p, o); };
    return registry;
} 

}
/**
 * @endcond
 */

/**
 * Registry of functions to be used by `validate()`.
 * Applications can extend **takane** by adding new validation functions for custom object types.
 */
inline std::unordered_map<std::string, std::function<void(const std::filesystem::path&, const Options&)> > validate_registry = internal_validate::default_registry();

/**
 * Validate an object in a subdirectory, based on the supplied object type.
 * This searches the `validate_registry` to find a validation function for the given type.
 *
 * @param path Path to a directory representing an object.
 * @param type Type of the object, typically determined from its `OBJECT` file.
 * @param options Validation options, mostly for input performance.
 */
inline void validate(const std::filesystem::path& path, const std::string& type, const Options& options) {
    if (!std::filesystem::exists(path) || std::filesystem::status(path).type() != std::filesystem::file_type::directory) {
        throw std::runtime_error("expected '" + path.string() + "' to be a directory");
    }

    auto vrIt = validate_registry.find(type);
    if (vrIt == validate_registry.end()) {
        throw std::runtime_error("no registered 'validate' function for object type '" + type + "' at '" + path.string() + "'");
    }

    (vrIt->second)(path, options);
}

/**
 * Validate an object in a subdirectory, using its `OBJECT` file to automatically determine the type.
 *
 * @param path Path to a directory containing an object.
 * @param options Validation options, mostly for input performance.
 */
inline void validate(const std::filesystem::path& path, const Options& options) {
    validate(path, read_object_type(path), options);
}

/**
 * Overload of `validate()` with default options.
 *
 * @param path Path to a directory containing an object.
 */
inline void validate(const std::filesystem::path& path) {
    validate(path, Options());
}

}

#endif
