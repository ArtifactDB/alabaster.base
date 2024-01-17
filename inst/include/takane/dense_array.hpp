#ifndef TAKANE_DENSE_ARRAY_HPP
#define TAKANE_DENSE_ARRAY_HPP

#include "ritsuko/hdf5/hdf5.hpp"
#include "ritsuko/ritsuko.hpp"

#include "utils_public.hpp"
#include "utils_array.hpp"

#include <vector>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <cstdint>

/**
 * @file dense_array.hpp
 * @brief Validation for dense arrays.
 */

namespace takane {

/**
 * @namespace takane::dense_array
 * @brief Definitions for dense arrays.
 */
namespace dense_array {

/**
 * @cond
 */
namespace internal {

inline bool is_transposed(const H5::Group& ghandle) {
    if (!ghandle.attrExists("transposed")) {
        return false;
    }

    auto attr = ghandle.openAttribute("transposed");
    if (!ritsuko::hdf5::is_scalar(attr)) {
        throw std::runtime_error("expected 'transposed' attribute to be a scalar");
    }
    if (ritsuko::hdf5::exceeds_integer_limit(attr, 32, true)) {
        throw std::runtime_error("expected 'transposed' attribute to have a datatype that fits in a 32-bit signed integer");
    }

    return ritsuko::hdf5::load_scalar_numeric_attribute<int32_t>(attr) != 0;
}

}
/**
 * @endcond
 */

/**
 * @param path Path to the directory containing a dense array.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    auto vstring = internal_json::extract_version_for_type(metadata.other, "dense_array");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version '" + vstring + "'");
    }

    auto handle = ritsuko::hdf5::open_file(path / "array.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "dense_array");
    internal::is_transposed(ghandle); // just a check, not used here.
    auto dhandle = ritsuko::hdf5::open_dataset(ghandle, "data");

    auto dspace = dhandle.getSpace();
    size_t ndims = dspace.getSimpleExtentNdims();
    if (ndims == 0) {
        throw std::runtime_error("expected 'data' array to have at least one dimension");
    }
    std::vector<hsize_t> extents(ndims);
    dspace.getSimpleExtentDims(extents.data());

    auto type = ritsuko::hdf5::open_and_load_scalar_string_attribute(ghandle, "type");
    if (type == "integer") {
        if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 32, true)) {
            throw std::runtime_error("expected integer array to have a datatype that fits into a 32-bit signed integer");
        }
    } else if (type == "boolean") {
        if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 32, true)) {
            throw std::runtime_error("expected boolean array to have a datatype that fits into a 32-bit signed integer");
        }
    } else if (type == "number") {
        if (ritsuko::hdf5::exceeds_float_limit(dhandle, 64)) {
            throw std::runtime_error("expected number array to have a datatype that fits into a 64-bit float");
        }
    } else if (type == "string") {
        if (!ritsuko::hdf5::is_utf8_string(dhandle)) {
            throw std::runtime_error("expected string array to have a datatype that can be represented by a UTF-8 encoded string");
        }
        ritsuko::hdf5::validate_nd_string_dataset(dhandle, extents, options.hdf5_buffer_size);
    } else {
        throw std::runtime_error("unknown array type '" + type + "'");
    }

    if (dhandle.attrExists("missing-value-placeholder")) {
        auto attr = dhandle.openAttribute("missing-value-placeholder");
        ritsuko::hdf5::check_missing_placeholder_attribute(dhandle, attr);
    }

    if (ghandle.exists("names")) {
        internal_array::check_dimnames(ghandle, "names", extents, options);
    }
}

/**
 * @param path Path to the directory containing a dense array.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return Extent of the first dimension.
 */
inline size_t height(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    auto handle = ritsuko::hdf5::open_file(path / "array.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "dense_array");

    auto dhandle = ritsuko::hdf5::open_dataset(ghandle, "data");
    auto dspace = dhandle.getSpace();
    size_t ndims = dspace.getSimpleExtentNdims();
    std::vector<hsize_t> extents(ndims);
    dspace.getSimpleExtentDims(extents.data());

    if (internal::is_transposed(ghandle)) {
        return extents.back();
    } else {
        return extents.front();
    }
}

/**
 * @param path Path to the directory containing a dense array.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return Dimensions of the array.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    auto handle = ritsuko::hdf5::open_file(path / "array.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "dense_array");

    auto dhandle = ritsuko::hdf5::open_dataset(ghandle, "data");
    auto dspace = dhandle.getSpace();
    size_t ndims = dspace.getSimpleExtentNdims();
    std::vector<hsize_t> extents(ndims);
    dspace.getSimpleExtentDims(extents.data());

    if (internal::is_transposed(ghandle)) {
        return std::vector<size_t>(extents.rbegin(), extents.rend());
    } else {
        return std::vector<size_t>(extents.begin(), extents.end());
    }
}

}

}

#endif
