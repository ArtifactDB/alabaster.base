#ifndef TAKANE_ATOMIC_VECTOR_HPP
#define TAKANE_ATOMIC_VECTOR_HPP

#include "ritsuko/hdf5/hdf5.hpp"

#include "utils_hdf5.hpp"

#include <stdexcept>

/**
 * @file atomic_vector.hpp
 * @brief Validation for atomic vectors.
 */

namespace takane {

/**
 * @namespace takane::atomic_vector
 * @brief Definitions for atomic vectors.
 */
namespace atomic_vector {

/**
 * @brief Parameters for validating the atomic vector file.
 */
struct Parameters {
    /**
     * Buffer size to use when reading values from the HDF5 file.
     */
    hsize_t buffer_size = 10000;
};

/**
 * @param path Path to the directory containing the atomic vector.
 * @param params Validation parameters.
 */
inline void validate(const std::string& path, Parameters params = Parameters()) try {
    H5::H5File handle(path + "/contents.h5", H5F_ACC_RDONLY);

    const char* parent = "atomic_vector";
    if (!handle.exists(parent) || handle.childObjType(parent) != H5O_TYPE_GROUP) {
        throw std::runtime_error("expected an 'atomic_vector' group");
    }
    auto ghandle = handle.openGroup(parent);

    auto vstring = ritsuko::hdf5::load_scalar_string_attribute(ghandle, "version");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    auto dhandle = ritsuko::hdf5::get_dataset(ghandle, "values");
    auto vlen = ritsuko::hdf5::get_1d_length(dhandle.getSpace(), false);
    auto type = ritsuko::hdf5::load_scalar_string_attribute(ghandle, "type");

    const char* missing_attr_name = "missing-value-placeholder";
    bool has_missing = dhandle.attrExists(missing_attr_name);
    H5::Attribute missing_attr; 
    if (has_missing) {
        missing_attr = ritsuko::hdf5::get_missing_placeholder_attribute(dhandle, missing_attr_name);
    }

    if (type == "integer") {
        if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 32, true)) {
            throw std::runtime_error("expected a datatype for 'values' that fits in a 32-bit signed integer");
        }
    } else if (type == "boolean") {
        if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 32, true)) {
            throw std::runtime_error("expected a datatype for 'values' that fits in a 32-bit signed integer");
        }
    } else if (type == "number") {
        if (ritsuko::hdf5::exceeds_float_limit(dhandle, 64)) {
            throw std::runtime_error("expected a datatype for 'values' that fits in a 64-bit float");
        }
    } else if (type == "string") {
        if (dhandle.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("expected a string datatype for 'values'");
        }

        std::string missing_value;
        if (has_missing) {
            missing_value = ritsuko::hdf5::load_scalar_string_attribute(missing_attr);
        }

        if (ghandle.attrExists("format")) {
            auto format = ritsuko::hdf5::load_scalar_string_attribute(ghandle, "format");
            validate_hdf5_string_format(dhandle, vlen, format, has_missing, missing_value, params.buffer_size);
        }
    } else {
        throw std::runtime_error("unsupported type '" + type + "'");
    }


    if (ghandle.exists("names")) {
        auto nhandle = ritsuko::hdf5::get_dataset(ghandle, "names");
        if (nhandle.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("'names' should be a string datatype class");
        }
        auto nlen = ritsuko::hdf5::get_1d_length(nhandle.getSpace(), false);
        if (vlen != nlen) {
            throw std::runtime_error("'names' and 'values' should have the same length");
        }
    }

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate an 'atomic_vector' at '" + path + "'; " + std::string(e.what()));
}

}

}

#endif
