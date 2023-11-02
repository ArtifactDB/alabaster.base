#ifndef TAKANE_HDF5_SPARSE_MATRIX_HPP
#define TAKANE_HDF5_SPARSE_MATRIX_HPP

#include "ritsuko/hdf5/hdf5.hpp"

#include "array.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <cstdint>

/**
 * @file hdf5_sparse_matrix.hpp
 * @brief Validation for HDF5 sparse matrices.
 */

namespace takane {

/**
 * @namespace takane::hdf5_sparse_matrix
 * @brief Definitions for HDF5 sparse matrices.
 */
namespace hdf5_sparse_matrix {

/**
 * @brief Parameters for validating a HDF5 sparse matrix file.
 */
struct Parameters {
    /**
     * @param group Name of the group containing the matrix.
     * @param dimensions Dimensions of the matrix.
     */
    Parameters(std::string group, std::array<size_t, 2> dimensions) : group(std::move(group)), dimensions(std::move(dimensions)) {}

    /**
     * Name of the group in the HDF5 file containing the matrix data.
     */
    std::string group;

    /**
     * Expected dimensions of the sparse matrix.
     * The first entry should contain the number of rows and the second should contain the number of columns.
     */
    std::array<size_t, 2> dimensions;

    /**
     * Expected type of the sparse matrix.
     */
    array::Type type = array::Type::INTEGER;

    /**
     * Whether the array has dimension names.
     */
    bool has_dimnames = false;

    /**
     * Name of the group containing the dimension names.
     */
    std::string dimnames_group;

    /**
     * Buffer size to use when reading values from the HDF5 file.
     */
    hsize_t buffer_size = 10000;

    /**
     * Version of this file specification.
     */
    int version = 2;
};

/**
 * @cond
 */
inline void validate_shape(const H5::Group& dhandle, const Parameters& params) try {
    const auto& dimensions = params.dimensions;
    if (!dhandle.exists("shape") || dhandle.childObjType("shape") != H5O_TYPE_DATASET) {
        throw std::runtime_error("expected a dataset");
    }

    auto shandle = dhandle.openDataSet("shape");
    if (params.version >= 3) {
        if (ritsuko::hdf5::exceeds_integer_limit(shandle, 64, true)) {
            throw std::runtime_error("expected the datatype to be a subset of a 64-bit signed integer");
        }
    } else {
        if (shandle.getTypeClass() != H5T_INTEGER) {
            throw std::runtime_error("expected an integer dataset");
        }
    }

    size_t len = ritsuko::hdf5::get_1d_length(shandle.getSpace(), false);
    if (len != 2) {
        throw std::runtime_error("expected the dataset to be of length 2");
    }

    int64_t dims[2];
    shandle.read(dims, H5::PredType::NATIVE_INT64);
    if (dims[0] < 0 || static_cast<size_t>(dims[0]) != params.dimensions[0]) {
        throw std::runtime_error("mismatch in first entry (expected " + std::to_string(dimensions[0]) + ", got " + std::to_string(dims[0]) + ")");
    }
    if (dims[1] < 0 || static_cast<size_t>(dims[1]) != params.dimensions[1]) {
        throw std::runtime_error("mismatch in second entry (expected " + std::to_string(dimensions[1]) + ", got " + std::to_string(dims[1]) + ")");
    }
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate sparse matrix shape at '" + ritsuko::hdf5::get_name(dhandle) + "/shape'; " + std::string(e.what()));
}

inline size_t validate_data(const H5::Group& dhandle, const Parameters& params) try {
    if (!dhandle.exists("data") || dhandle.childObjType("data") != H5O_TYPE_DATASET) {
        throw std::runtime_error("expected a dataset");
    }

    auto ddhandle = dhandle.openDataSet("data");
    size_t num_nonzero = ritsuko::hdf5::get_1d_length(ddhandle.getSpace(), false);

    if (params.type == array::Type::INTEGER || params.type == array::Type::BOOLEAN) {
        if (params.version >= 3) {
            if (ritsuko::hdf5::exceeds_integer_limit(ddhandle, 32, true)) {
                throw std::runtime_error("expected datatype to be a subset of a 32-bit signed integer");
            }
        } else {
            if (ddhandle.getTypeClass() != H5T_INTEGER) {
                throw std::runtime_error("expected an integer dataset");
            }
        }
    } else if (params.type == array::Type::NUMBER) {
        if (params.version >= 3) {
            if (ritsuko::hdf5::exceeds_float_limit(ddhandle, 64)) {
                throw std::runtime_error("expected datatype to be a subset of a 64-bit float");
            }
        } else {
            auto tclass = ddhandle.getTypeClass();
            if (tclass != H5T_INTEGER && tclass != H5T_FLOAT) {
                throw std::runtime_error("expected an integer or floating-point dataset");
            }
        }
    } else {
        throw std::runtime_error("unexpected array type for a sparse matrix");
    }

    if (params.version >= 2) {
        const char* missing_attr = "missing-value-placeholder";
        if (ddhandle.attrExists(missing_attr)) {
            ritsuko::hdf5::get_missing_placeholder_attribute(ddhandle, missing_attr);
        }
    }

    return num_nonzero;
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate sparse matrix data at '" + ritsuko::hdf5::get_name(dhandle) + "/data'; " + std::string(e.what()));
}

inline std::vector<uint64_t> validate_indptrs(const H5::Group& dhandle, size_t primary_dim, size_t num_nonzero, const Parameters& params) try {
    if (!dhandle.exists("indptr") || dhandle.childObjType("indptr") != H5O_TYPE_DATASET) {
        throw std::runtime_error("expected a dataset");
    }

    auto iphandle = dhandle.openDataSet("indptr");
    if (params.version >= 3) {
        if (ritsuko::hdf5::exceeds_integer_limit(iphandle, 64, false)) {
            throw std::runtime_error("expected datatype to be a subset of a 64-bit unsigned integer");
        }
    } else {
        if (iphandle.getTypeClass() != H5T_INTEGER) {
            throw std::runtime_error("expected an integer dataset");
        }
    }

    size_t len = ritsuko::hdf5::get_1d_length(iphandle.getSpace(), false);
    if (len != primary_dim + 1) {
        throw std::runtime_error("dataset should have length equal to the primary dimension extent plus 1");
    }

    std::vector<uint64_t> indptrs(len);
    iphandle.read(indptrs.data(), H5::PredType::NATIVE_UINT64);

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
    throw std::runtime_error("failed to validate sparse matrix pointers at '" + ritsuko::hdf5::get_name(dhandle) + "/indptr'; " + std::string(e.what()));
}

inline void validate_indices(const H5::Group& dhandle, const std::vector<uint64_t>& indptrs, uint64_t secondary_dim, const Parameters& params) try {
    if (!dhandle.exists("indices") || dhandle.childObjType("indices") != H5O_TYPE_DATASET) {
        throw std::runtime_error("expected a dataset");
    }

    auto ixhandle = dhandle.openDataSet("indices");
    auto len = ritsuko::hdf5::get_1d_length(ixhandle.getSpace(), false);
    if (indptrs.back() != len) {
        throw std::runtime_error("dataset length should be equal to the number of non-zero elements (expected " + std::to_string(indptrs.back()) + ", got " + std::to_string(len) + ")");
    }

    if (params.version >= 3) {
        if (ritsuko::hdf5::exceeds_integer_limit(ixhandle, 64, false)) {
            throw std::runtime_error("expected datatype to be a subset of a 64-bit unsigned integer");
        }
    } else {
        if (ixhandle.getTypeClass() != H5T_INTEGER) {
            throw std::runtime_error("expected an integer dataset");
        }
    }

    auto block_size = ritsuko::hdf5::pick_1d_block_size(ixhandle.getCreatePlist(), len, params.buffer_size);
    std::vector<uint64_t> buffer(block_size);
    size_t which_ptr = 0;
    uint64_t last_index = 0;
    auto limit = indptrs[0];

    ritsuko::hdf5::iterate_1d_blocks(
        len,
        block_size,
        [&](hsize_t position, hsize_t len, const H5::DataSpace& memspace, const H5::DataSpace& dataspace) {
            buffer.resize(len);
            ixhandle.read(buffer.data(), H5::PredType::NATIVE_UINT64, memspace, dataspace);

            for (auto bIt = buffer.begin(); bIt != buffer.end(); ++bIt, ++position) {
                if (*bIt >= secondary_dim) {
                    throw std::runtime_error("out-of-range index (" + std::to_string(*bIt) + ")");
                }

                if (position == limit) {
                    // No need to check if there are more or fewer elements
                    // than expected, as we already know that indptr.back()
                    // is equal to the number of non-zero elements.
                    do {
                        ++which_ptr;
                        limit = indptrs[which_ptr];
                    } while (position == limit);

                } else if (last_index >= *bIt) {
                    throw std::runtime_error("indices should be strictly increasing");
                }

                last_index = *bIt;
            }
        }
    );
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate sparse matrix indices at '" + ritsuko::hdf5::get_name(dhandle) + "/indices'; " + std::string(e.what()));
}
/**
 * @endcond
 */

/**
 * Checks if a HDF5 sparse matrix is correctly formatted.
 * An error is raised if the file does not meet the specifications.
 *
 * @param handle Handle to the HDF5 file containing the matrix.
 * @param params Parameters for validation.
 */
inline void validate(const H5::H5File& handle, const Parameters& params) {
    const auto& group = params.group;    
    if (!handle.exists(group) || handle.childObjType(group) != H5O_TYPE_GROUP) {
        throw std::runtime_error("expected a '" + group + "' group");
    }
    auto dhandle = handle.openGroup(group);

    validate_shape(dhandle, params);
    size_t num_nonzero = validate_data(dhandle, params);
    std::vector<uint64_t> indptrs = validate_indptrs(dhandle, params.dimensions[1], num_nonzero, params);
    validate_indices(dhandle, indptrs, params.dimensions[0], params);

    if (params.has_dimnames) {
        array::check_dimnames(handle, params.dimnames_group, params.dimensions);
    }
}

/**
 * Overload for `hdf5_sparse_matrix::validate()` that accepts a file path.
 *
 * @param path Path to the HDF5 file.
 * @param params Parameters for validation.
 */
inline void validate(const char* path, const Parameters& params) {
    H5::H5File handle(path, H5F_ACC_RDONLY);
    validate(handle, params);
}


}

}

#endif
