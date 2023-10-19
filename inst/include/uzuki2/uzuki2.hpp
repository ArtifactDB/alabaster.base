#ifndef UZUKI2_UZUKI2_HPP
#define UZUKI2_UZUKI2_HPP

/**
 * @file uzuki2.hpp
 * @brief Umbrella header for the **uzuki2** library.
 */

/**
 * @namespace uzuki2
 * @brief Parse an R list from a HDF5 or JSON file.
 */

#if __has_include("H5Cpp.h")
#include "parse_hdf5.hpp"
#endif
#include "parse_json.hpp"

#endif
