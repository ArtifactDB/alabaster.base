#ifndef TAKANE_GENOMIC_RANGES_LIST_HPP
#define TAKANE_GENOMIC_RANGES_LIST_HPP

#include "H5Cpp.h"

#include <filesystem>
#include <stdexcept>
#include <string>

#include "utils_public.hpp"
#include "utils_compressed_list.hpp"

/**
 * @file genomic_ranges_list.hpp
 * @brief Validation for genomic ranges lists.
 */

namespace takane {

namespace genomic_ranges_list {

/**
 * @param path Path to the directory containing the genomic ranges list.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    internal_compressed_list::validate_directory<false>(path, "genomic_ranges_list", "genomic_ranges", metadata, options);
}

/**
 * @param path Path to a directory containing an genomic ranges list.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return The length of the list.
 */
inline size_t height(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    return internal_compressed_list::height(path, "genomic_ranges_list", metadata, options);
}

}

}

#endif
