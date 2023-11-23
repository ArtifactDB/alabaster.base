#ifndef TAKANE_ATOMIC_VECTOR_LIST_HPP
#define TAKANE_ATOMIC_VECTOR_LIST_HPP

#include "H5Cpp.h"

#include <filesystem>
#include <stdexcept>
#include <string>

#include "utils_public.hpp"
#include "utils_compressed_list.hpp"

/**
 * @file atomic_vector_list.hpp
 * @brief Validation for atomic vector lists.
 */

namespace takane {

namespace atomic_vector_list {

/**
 * @param path Path to the directory containing the atomic vector list.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const Options& options) try {
    internal_compressed_list::validate_directory<false>(path, "atomic_vector_list", "atomic_vector", options);
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate an 'atomic_vector_list' object at '" + path.string() + "'; " + std::string(e.what()));
}

/**
 * @param path Path to a directory containing an atomic vector list.
 * @param options Validation options, mostly for input performance.
 * @return The length of the list.
 */
inline size_t height(const std::filesystem::path& path, const Options& options) {
    return internal_compressed_list::height(path, "atomic_vector_list", options);
}

}

}

#endif
