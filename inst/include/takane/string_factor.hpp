#ifndef TAKANE_STRING_FACTOR_HPP
#define TAKANE_STRING_FACTOR_HPP

#include "ritsuko/hdf5/hdf5.hpp"

#include "utils_hdf5.hpp"

#include <stdexcept>

/**
 * @file string_factor.hpp
 * @brief Validation for string factors.
 */

namespace takane {

/**
 * @namespace takane::string_factor
 * @brief Definitions for string factors.
 */
namespace string_factor {

/**
 * @brief Parameters for validating the string factor file.
 */
struct Parameters {
    /**
     * Buffer size to use when reading values from the HDF5 file.
     */
    hsize_t buffer_size = 10000;
};

/**
 * @param path Path to the directory containing the string factor.
 * @param params Validation parameters.
 */
inline void validate(const std::string& path, Parameters params = Parameters()) try {
    H5::H5File handle(path + "/contents.h5", H5F_ACC_RDONLY);

    const char* parent = "string_factor";
    if (!handle.exists(parent) || handle.childObjType(parent) != H5O_TYPE_GROUP) {
        throw std::runtime_error("expected a 'string_factor' group");
    }
    auto ghandle = handle.openGroup(parent);

    auto vstring = ritsuko::hdf5::load_scalar_string_attribute(ghandle, "version");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    if (ghandle.attrExists("ordered")) {
        auto oattr = ritsuko::hdf5::get_scalar_attribute(ghandle, "ordered");
        if (ritsuko::hdf5::exceeds_integer_limit(oattr, 32, true)) {
            throw std::runtime_error("expected a datatype for the 'ordered' attribute that fits in a 32-bit signed integer");
        }
    }

    // Number of levels.
    size_t num_levels = validate_hdf5_factor_levels(ghandle, "levels", params.buffer_size);
    size_t num_codes = validate_hdf5_factor_codes(ghandle, "codes", num_levels, params.buffer_size);

    if (ghandle.exists("names")) {
        auto nhandle = ritsuko::hdf5::get_dataset(ghandle, "names");
        if (nhandle.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("'names' should be a string datatype class");
        }
        auto nlen = ritsuko::hdf5::get_1d_length(nhandle.getSpace(), false);
        if (num_codes != nlen) {
            throw std::runtime_error("'names' and 'codes' should have the same length");
        }
    }

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate a 'string_factor' at '" + path + "'; " + std::string(e.what()));
}

}

}

#endif
