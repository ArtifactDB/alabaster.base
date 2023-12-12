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

inline void validate_string_contents(const H5::DataSet& dhandle, const std::vector<hsize_t>& data_extent, hsize_t buffer_size) {
    auto stype = dhandle.getDataType();
    if (!stype.isVariableStr()) {
        return;
    }

    for (auto ex : data_extent) {
        if (ex == 0) {
            return;
        }
    }

    hsize_t ndims = data_extent.size();
    std::vector<hsize_t> chunk_extent(ndims, 1);
    auto cplist = dhandle.getCreatePlist();
    if (cplist.getLayout() == H5D_CHUNKED) {
        cplist.getChunk(chunk_extent.size(), chunk_extent.data());
    }

    // Scaling up the block size as much as possible. We start from the
    // fastest-changing dimension (i.e., the last one in HDF5) and increase it,
    // and then we move onto the next-fastest dimension, and so on until the
    // buffer size is exhausted.
    auto block_extent = chunk_extent;
    hsize_t block_size = 1;
    for (hsize_t d = 0; d < ndims; ++d) {
        block_extent[d] = std::min(block_extent[d], data_extent[d]); // should be a no-op, but we do this just in case.
        block_size *= block_extent[d];
    }

    for (hsize_t i = ndims; i > 0; --i) {
        int multiple = buffer_size / block_size;
        if (multiple <= 1) {
            break;
        }
        auto d = i - 1;
        block_size /= block_extent[d];
        block_extent[d] = std::min(data_extent[d], block_extent[d] * multiple);
        block_size *= block_extent[d];
    }

    // Now iterating through the array.
    std::vector<hsize_t> starts(ndims), counts(block_extent.begin(), block_extent.end());
    std::vector<char*> buffer(block_size);
    hsize_t buffer_length = block_size;

    H5::DataSpace mspace, dspace(ndims, data_extent.data());
    bool finished = false;

    while (!finished) {
        buffer.resize(buffer_length);
        dspace.selectHyperslab(H5S_SELECT_SET, counts.data(), starts.data());
        mspace.setExtentSimple(ndims, counts.data());

        [[maybe_unused]] ritsuko::hdf5::VariableStringCleaner stream(stype.getId(), mspace.getId(), buffer.data());
        dhandle.read(buffer.data(), stype, mspace, dspace);
        for (auto x : buffer) {
            if (x == NULL) {
                throw std::runtime_error("detected NULL pointer in a variable-length string dataset");
            }
        }

        // Attempting a shift from the last dimension as this is the fastest-changing.
        for (hsize_t i = ndims; i > 0; --i) {
            auto d = i - 1;
            starts[d] += block_extent[d];
            if (starts[d] >= data_extent[d]) {
                if (d == 0) {
                    finished = true;
                } else {
                    starts[d] = 0;
                    buffer_length /= counts[d];
                    counts[d] = std::min(data_extent[d], block_extent[d]);
                    buffer_length *= counts[d];
                }
            } else {
                buffer_length /= counts[d];
                counts[d] = std::min(data_extent[d] - starts[d], block_extent[d]);
                buffer_length *= counts[d];
                break;
            }
        }
    }
}

}
/**
 * @endcond
 */

/**
 * @param path Path to the directory containing a dense array.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, mostly related to reading performance.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, const Options& options) {
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
        if (dhandle.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("expected string array to have a string datatype class");
        }
        internal::validate_string_contents(dhandle, extents, options.hdf5_buffer_size);
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
 * @param options Validation options, mostly related to reading performance.
 * @return Extent of the first dimension.
 */
inline size_t height(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] const Options& options) {
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
 * @param options Validation options, mostly related to reading performance.
 * @return Dimensions of the array.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] const Options& options) {
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
