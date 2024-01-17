#ifndef TAKANE_DATA_FRAME_LIST_HPP
#define TAKANE_DATA_FRAME_LIST_HPP

#include "H5Cpp.h"

#include <filesystem>
#include <stdexcept>
#include <string>

#include "utils_public.hpp"
#include "utils_compressed_list.hpp"

/**
 * @file data_frame_list.hpp
 * @brief Validation for data frame lists.
 */

namespace takane {

namespace data_frame_list {

/**
 * @param path Path to the directory containing the data frame list.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    internal_compressed_list::validate_directory<true>(path, "data_frame_list", "DATA_FRAME", metadata, options);
}

/**
 * @param path Path to a directory containing an data frame list.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return The length of the list.
 */
inline size_t height(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    return internal_compressed_list::height(path, "data_frame_list", metadata, options);
}

}

}

#endif
