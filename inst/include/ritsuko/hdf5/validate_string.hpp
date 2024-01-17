#ifndef RITSUKO_HDF5_VALIDATE_STRING_HPP
#define RITSUKO_HDF5_VALIDATE_STRING_HPP

#include <string>
#include <vector>
#include <stdexcept>

#include "H5Cpp.h"

#include "get_name.hpp"
#include "get_1d_length.hpp"
#include "get_dimensions.hpp"
#include "pick_1d_block_size.hpp"
#include "pick_nd_block_dimensions.hpp"
#include "IterateNdDataset.hpp"
#include "_strings.hpp"

/**
 * @file validate_string.hpp
 * @brief Helper functions to validate strings.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Check that a scalar string dataset is valid.
 * Currently, this involves checking that there are no `NULL` entries for variable-length string datatypes.
 * For fixed-width string datasets, this function is a no-op.
 *
 * @param handle Handle to the HDF5 string dataset.
 */
inline void validate_scalar_string_dataset(const H5::DataSet& handle) {
    auto dtype = handle.getDataType();
    if (!dtype.isVariableStr()) {
        return;
    }

    char* vptr;
    handle.read(&vptr, dtype);
    auto dspace = handle.getSpace(); // don't set as temporary in constructor below, otherwise it gets destroyed and the ID invalidated.
    [[maybe_unused]] VariableStringCleaner deletor(dtype.getId(), dspace.getId(), &vptr);
    if (vptr == NULL) {
        throw std::runtime_error("detected a NULL pointer for a variable length string in '" + get_name(handle) + "'");
    }
}

/**
 * Check that a 1-dimensional string dataset is valid.
 * Currently, this involves checking that there are no `NULL` entries for variable-length string datatypes.
 * For fixed-width string datasets, this function is a no-op.
 *
 * @param handle Handle to the HDF5 string dataset.
 * @param full_length Length of the dataset as a 1-dimensional vector.
 * @param buffer_size Size of the buffer for holding loaded strings.
 */
inline void validate_1d_string_dataset(const H5::DataSet& handle, hsize_t full_length, hsize_t buffer_size) {
    auto dtype = handle.getDataType();
    if (!dtype.isVariableStr()) {
        return;
    }

    hsize_t block_size = pick_1d_block_size(handle.getCreatePlist(), full_length, buffer_size);
    H5::DataSpace mspace(1, &block_size), dspace(1, &full_length);
    std::vector<char*> buffer(block_size);

    for (hsize_t i = 0; i < full_length; i += block_size) {
        auto available = std::min(full_length - i, block_size);
        constexpr hsize_t zero = 0;
        mspace.selectHyperslab(H5S_SELECT_SET, &available, &zero);
        dspace.selectHyperslab(H5S_SELECT_SET, &available, &i);

        handle.read(buffer.data(), dtype, mspace, dspace);
        [[maybe_unused]] VariableStringCleaner deletor(dtype.getId(), mspace.getId(), buffer.data());
        for (hsize_t j = 0; j < available; ++j) {
            if (buffer[j] == NULL) {
                throw std::runtime_error("detected a NULL pointer for a variable length string in '" + get_name(handle) + "'");
            }
        }
    }
}

/**
 * Overload for `validate_1d_string_dataset()` that automatically determines its length via `get_1d_length()`.
 * @param handle Handle to the HDF5 string dataset.
 * @param buffer_size Size of the buffer for holding loaded strings.
 */
inline void validate_1d_string_dataset(const H5::DataSet& handle, hsize_t buffer_size) {
    validate_1d_string_dataset(handle, get_1d_length(handle, false), buffer_size);
}

/**
 * Check that an N-dimensional string dataset is valid.
 * Currently, this involves checking that there are no `NULL` entries for variable-length string datatypes.
 * For fixed-width string datasets, this function is a no-op.
 *
 * @param handle Handle to the HDF5 string dataset.
 * @param dimensions Dimensions of the dataset.
 * @param buffer_size Size of the buffer for holding loaded strings.
 */
inline void validate_nd_string_dataset(const H5::DataSet& handle, const std::vector<hsize_t>& dimensions, hsize_t buffer_size) {
    auto stype = handle.getDataType();
    if (!stype.isVariableStr()) {
        return;
    }

    auto blocks = pick_nd_block_dimensions(handle.getCreatePlist(), dimensions, buffer_size);
    IterateNdDataset iter(dimensions, blocks);
    std::vector<char*> buffer;

    while (!iter.finished()) {
        buffer.resize(iter.current_block_size());

        // Scope this to ensure that 'mspace' doesn't get changed by
        // 'iter.next()' before the destructor is called.
        {
            const auto& mspace = iter.memory_space();
            [[maybe_unused]] VariableStringCleaner stream(stype.getId(), mspace.getId(), buffer.data());
            handle.read(buffer.data(), stype, mspace, iter.file_space());
            for (auto x : buffer) {
                if (x == NULL) {
                    throw std::runtime_error("detected NULL pointer in a variable-length string dataset");
                }
            }
        }

        iter.next();
    }
}

/**
 * Overload for `validate_nd_string_dataset()` that automatically determines the dimensions.
 * @param handle Handle to the HDF5 string dataset.
 * @param buffer_size Size of the buffer for holding loaded strings.
 */
inline void validate_nd_string_dataset(const H5::DataSet& handle, hsize_t buffer_size) {
    auto dimensions = get_dimensions(handle, false);
    validate_nd_string_dataset(handle, dimensions, buffer_size);
}

/**
 * Check that a scalar string attribute is valid.
 * Currently, this involves checking that there are no `NULL` entries for variable-length string datatypes.
 * For fixed-width string attributes, this function is a no-op.
 *
 * @param attr Handle to the HDF5 string attribute.
 */
inline void validate_scalar_string_attribute(const H5::Attribute& attr) {
    auto dtype = attr.getDataType();
    if (!dtype.isVariableStr()) {
        return;
    }

    auto mspace = attr.getSpace();
    char* buffer;
    attr.read(dtype, &buffer);
    [[maybe_unused]] VariableStringCleaner deletor(dtype.getId(), mspace.getId(), &buffer);
    if (buffer == NULL) {
        throw std::runtime_error("detected a NULL pointer for a variable length string attribute");
    }
}

/**
 * Check that a 1-dimensional string attribute is valid.
 * Currently, this involves checking that there are no `NULL` entries for variable-length string datatypes.
 * For fixed-width string attributes, this function is a no-op.
 *
 * @param attr Handle to the HDF5 string attribute.
 * @param full_length Length of the attribute as a 1-dimensional vector.
 */
inline void validate_1d_string_attribute(const H5::Attribute& attr, hsize_t full_length) {
    auto dtype = attr.getDataType();
    if (!dtype.isVariableStr()) {
        return;
    }

    auto mspace = attr.getSpace();
    std::vector<char*> buffer(full_length);
    attr.read(dtype, buffer.data());
    [[maybe_unused]] VariableStringCleaner deletor(dtype.getId(), mspace.getId(), buffer.data());
    for (hsize_t i = 0; i < full_length; ++i) {
        if (buffer[i] == NULL) {
            throw std::runtime_error("detected a NULL pointer for a variable length string attribute");
        }
    }
}

/**
 * Overload for `validate_1d_string_attribute()` that automatically determines its length via `get_1d_length()`.
 * @param attr Handle to the HDF5 string attribute.
 */
inline void validate_1d_string_attribute(const H5::Attribute& attr) {
    validate_1d_string_attribute(attr, get_1d_length(attr, false));
}

}

}

#endif
