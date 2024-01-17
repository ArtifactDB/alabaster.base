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
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    internal_compressed_list::validate_directory<false>(path, "atomic_vector_list", "atomic_vector", metadata, options);
}

/**
 * @param path Path to a directory containing an atomic vector list.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return The length of the list.
 */
inline size_t height(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    return internal_compressed_list::height(path, "atomic_vector_list", metadata, options);
}

}

}

#endif
