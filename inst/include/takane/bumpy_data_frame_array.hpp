#ifndef TAKANE_BUMPY_DATA_FRAME_ARRAY_HPP
#define TAKANE_BUMPY_DATA_FRAME_ARRAY_HPP

#include "H5Cpp.h"

#include <filesystem>
#include <stdexcept>
#include <string>

#include "utils_public.hpp"
#include "utils_bumpy_array.hpp"

/**
 * @file bumpy_data_frame_array.hpp
 * @brief Validation for bumpy data frame matrices.
 */

namespace takane {

namespace bumpy_data_frame_array {

/**
 * @param path Path to the directory containing the bumpy data frame array.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    internal_bumpy_array::validate_directory<true>(path, "bumpy_data_frame_array", "DATA_FRAME", metadata, options);
}

/**
 * @param path Path to a directory containing an bumpy data frame array.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return The height (i.e., first dimension extent) of the array.
 */
inline size_t height(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    return internal_bumpy_array::height(path, "bumpy_data_frame_array", metadata, options);
}

/**
 * @param path Path to a directory containing an bumpy data frame array.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return Vector containing the dimensions of the array.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    return internal_bumpy_array::dimensions(path, "bumpy_data_frame_array", metadata, options);
}

}

}

#endif
