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
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const Options& options) try {
    internal_compressed_list::validate_directory<true>(path, "data_frame_list", "DATA_FRAME", options);
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate an 'data_frame_list' object at '" + path.string() + "'; " + std::string(e.what()));
}

/**
 * @param path Path to a directory containing an data frame list.
 * @param options Validation options, mostly for input performance.
 * @return The length of the list.
 */
inline size_t height(const std::filesystem::path& path, const Options& options) {
    return internal_compressed_list::height(path, "data_frame_list", options);
}

}

}

#endif
