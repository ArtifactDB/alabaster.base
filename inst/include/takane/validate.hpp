#ifndef TAKANE_VALIDATE_HPP
#define TAKANE_VALIDATE_HPP

#include <functional>
#include <string>
#include <stdexcept>
#include <filesystem>

#include "utils_public.hpp"
#include "atomic_vector.hpp"
#include "string_factor.hpp"

/**
 * @file validate.hpp
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
    return registry;
} 

}
/**
 * @endcond
 */

/**
 * Registry of validation functions, to be used by `validate()`.
 */
inline std::unordered_map<std::string, std::function<void(const std::filesystem::path&, const Options& )> > validation_registry = internal_validate::default_registry();

/**
 * Validate an object in a subdirectory, based on the supplied object type.
 *
 * @param path Path to a directory representing an object.
 * @param type Type of the object, typically determined from its `OBJECT` file.
 * @param options Validation options, mostly for input performance.
 */
inline void validate(const std::filesystem::path& path, const std::string& type, const Options& options) {
    if (std::filesystem::status(path).type() != std::filesystem::file_type::directory) {
        throw std::runtime_error("expected '" + path.string() + "' to be a directory");
    }

    auto vrIt = validation_registry.find(type);
    if (vrIt != validation_registry.end()) {
        (vrIt->second)(path, options);
    } else {
        throw std::runtime_error("failed to find a validation function for object type '" + type + "' at '" + path.string() + "'");
    }
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
