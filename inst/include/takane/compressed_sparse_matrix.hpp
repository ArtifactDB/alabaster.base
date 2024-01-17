#ifndef TAKANE_HDF5_SPARSE_MATRIX_HPP
#define TAKANE_HDF5_SPARSE_MATRIX_HPP

#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include "utils_public.hpp"
#include "utils_array.hpp"
#include "utils_json.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <cstdint>
#include <array>
#include <vector>

/**
 * @file compressed_sparse_matrix.hpp
 * @brief Validation for compressed sparse matrices.
 */

namespace takane {

/**
 * @namespace takane::compressed_sparse_matrix
 * @brief Definitions for compressed sparse matrices.
 */
namespace compressed_sparse_matrix {

/**
 * @cond
 */
namespace internal {

inline std::array<uint64_t, 2> validate_shape(const H5::Group& handle, const Options&) try {
    auto shandle = ritsuko::hdf5::open_dataset(handle, "shape");
    if (ritsuko::hdf5::exceeds_integer_limit(shandle, 64, false)) {
        throw std::runtime_error("expected the datatype to be a subset of a 64-bit unsigned integer");
    }

    size_t len = ritsuko::hdf5::get_1d_length(shandle, false);
    if (len != 2) {
        throw std::runtime_error("expected the dataset to be of length 2");
    }

    std::array<uint64_t, 2> output;
    shandle.read(output.data(), H5::PredType::NATIVE_UINT64);
    return output;

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate sparse matrix shape at '" + ritsuko::hdf5::get_name(handle) + "/shape'; " + std::string(e.what()));
}

inline size_t validate_data(const H5::Group& handle, const Options&) try {
    auto dhandle = ritsuko::hdf5::open_dataset(handle, "data");

    auto type = ritsuko::hdf5::open_and_load_scalar_string_attribute(handle, "type");
    if (type == "integer") {
        if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 32, true)) {
            throw std::runtime_error("expected an integer 'data' to fit inside a 32-bit signed integer");
        }
    } else if (type == "boolean") {
        if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 32, true)) {
            throw std::runtime_error("expected a boolean 'data' to fit inside a 32-bit signed integer");
        }
    } else if (type == "number") {
        if (ritsuko::hdf5::exceeds_float_limit(dhandle, 64)) {
            throw std::runtime_error("expected a number 'data' to fit inside a 64-bit float");
        }
    } else {
        throw std::runtime_error("unknown matrix type '" + type + "'");
    }

    if (dhandle.attrExists("missing-value-placeholder")) {
        auto attr = dhandle.openAttribute("missing-value-placeholder");
        ritsuko::hdf5::check_missing_placeholder_attribute(dhandle, attr);
    }

    return ritsuko::hdf5::get_1d_length(dhandle, false);
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate sparse matrix data at '" + ritsuko::hdf5::get_name(handle) + "/data'; " + std::string(e.what()));
}

inline std::vector<uint64_t> validate_indptrs(const H5::Group& handle, size_t primary_dim, size_t num_nonzero) try {
    auto dhandle = ritsuko::hdf5::open_dataset(handle, "indptr");
    if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 64, false)) {
        throw std::runtime_error("expected datatype to be a subset of a 64-bit unsigned integer");
    }

    size_t len = ritsuko::hdf5::get_1d_length(dhandle, false);
    if (len != primary_dim + 1) {
        throw std::runtime_error("dataset should have length equal to the primary dimension extent plus 1");
    }

    std::vector<uint64_t> indptrs(len);
    dhandle.read(indptrs.data(), H5::PredType::NATIVE_UINT64);

    if (indptrs[0] != 0) {
        throw std::runtime_error("first entry should be zero");
    }
    if (indptrs.back() != num_nonzero) {
        throw std::runtime_error("last entry should equal the number of non-zero elements");
    }

    for (size_t i = 1; i < len; ++i) {
        if (indptrs[i] < indptrs[i-1]) {
            throw std::runtime_error("pointers should be sorted in increasing order");
        }
    }

    return indptrs;
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate sparse matrix pointers at '" + ritsuko::hdf5::get_name(handle) + "/indptr'; " + std::string(e.what()));
}

inline void validate_indices(const H5::Group& handle, const std::vector<uint64_t>& indptrs, uint64_t secondary_dim, const Options& options) try {
    auto dhandle = ritsuko::hdf5::open_dataset(handle, "indices");
    if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 64, false)) {
        throw std::runtime_error("expected datatype to be a subset of a 64-bit unsigned integer");
    }

    auto len = ritsuko::hdf5::get_1d_length(dhandle.getSpace(), false);
    if (indptrs.back() != len) {
        throw std::runtime_error("dataset length should be equal to the number of non-zero elements (expected " + std::to_string(indptrs.back()) + ", got " + std::to_string(len) + ")");
    }

    size_t which_ptr = 0;
    uint64_t last_index = 0;
    hsize_t limit = indptrs[0];
    ritsuko::hdf5::Stream1dNumericDataset<uint64_t> stream(&dhandle, len, options.hdf5_buffer_size);

    for (hsize_t i = 0; i < len; ++i, stream.next()) {
        auto x = stream.get();
        if (x >= secondary_dim) {
            throw std::runtime_error("out-of-range index (" + std::to_string(x) + ")");
        }

        if (i == limit) {
            // No need to check if there are more or fewer elements
            // than expected, as we already know that indptr.back()
            // is equal to the number of non-zero elements.
            do {
                ++which_ptr;
                limit = indptrs[which_ptr];
            } while (i == limit);
        } else if (last_index >= x) {
            throw std::runtime_error("indices should be strictly increasing");
        }

        last_index = x;
    }

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate sparse matrix indices at '" + ritsuko::hdf5::get_name(handle) + "/indices'; " + std::string(e.what()));
}

}
/**
 * @endcond
 */

/**
 * @param path Path to a directory containing a compressed sparse matrix.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    const auto& vstring = internal_json::extract_version_for_type(metadata.other, "compressed_sparse_matrix");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version '" + vstring + "'");
    }

    auto handle = ritsuko::hdf5::open_file(path / "matrix.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "compressed_sparse_matrix");
    auto layout = ritsuko::hdf5::open_and_load_scalar_string_attribute(ghandle, "layout");
    size_t primary = 0;
    if (layout == "CSC") {
        primary = 1;
    } else if (layout != "CSR") {
        throw std::runtime_error("'layout' attribute must be one of 'CSC' or 'CSR'");
    }

    auto shape = internal::validate_shape(ghandle, options);
    size_t num_nonzero = internal::validate_data(ghandle, options);
    auto indptrs = internal::validate_indptrs(ghandle, shape[primary], num_nonzero);
    internal::validate_indices(ghandle, indptrs, shape[1 - primary], options);

    if (ghandle.exists("names")) {
        std::vector<hsize_t> dims(shape.begin(), shape.end());
        internal_array::check_dimnames(ghandle, "names", dims, options);
    }
}

/**
 * @param path Path to the directory containing a compressed sparse matrix.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return Number of rows in the matrix.
 */
inline size_t height(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    auto handle = ritsuko::hdf5::open_file(path / "matrix.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "compressed_sparse_matrix");
    auto shandle = ritsuko::hdf5::open_dataset(ghandle, "shape");
    std::array<uint64_t, 2> output;
    shandle.read(output.data(), H5::PredType::NATIVE_UINT64);
    return output.front();
}

/**
 * @param path Path to the directory containing a compressed sparse matrix.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return Dimensions of the matrix.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    auto handle = ritsuko::hdf5::open_file(path / "matrix.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "compressed_sparse_matrix");
    auto shandle = ritsuko::hdf5::open_dataset(ghandle, "shape");
    std::array<uint64_t, 2> output;
    shandle.read(output.data(), H5::PredType::NATIVE_UINT64);
    return std::vector<size_t>(output.begin(), output.end());
}

}

}

#endif
