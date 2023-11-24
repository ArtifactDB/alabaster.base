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
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const Options& options) try {
    internal_compressed_list::validate_directory<false>(path, "genomic_ranges_list", "genomic_ranges", options);
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate an 'genomic_ranges_list' object at '" + path.string() + "'; " + std::string(e.what()));
}

/**
 * @param path Path to a directory containing an genomic ranges list.
 * @param options Validation options, mostly for input performance.
 * @return The length of the list.
 */
inline size_t height(const std::filesystem::path& path, const Options& options) {
    return internal_compressed_list::height(path, "genomic_ranges_list", options);
}

}

}

#endif
