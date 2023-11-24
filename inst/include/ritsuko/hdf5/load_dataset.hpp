#ifndef RITSUKO_HDF5_LOAD_DATASET_HPP
#define RITSUKO_HDF5_LOAD_DATASET_HPP

#include <string>
#include <vector>
#include <stdexcept>

#include "H5Cpp.h"

#include "get_name.hpp"
#include "Stream1dStringDataset.hpp"
#include "_strings.hpp"

/**
 * @file load_dataset.hpp
 * @brief Helper functions to load datasets.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Load a scalar string dataset into a single string.
 * @param handle Handle to the HDF5 scalar dataset.
 * @return String containing the contents of the sole dataset entry.
 */
inline std::string load_scalar_string_dataset(const H5::DataSet& handle) {
    auto dtype = handle.getDataType();
    if (dtype.isVariableStr()) {
        char* vptr;
        handle.read(&vptr, dtype);
        auto dspace = handle.getSpace(); // don't set as temporary in constructor below, otherwise it gets destroyed and the ID invalidated.
        [[maybe_unused]] VariableStringCleaner deletor(dtype.getId(), dspace.getId(), &vptr);
        if (vptr == NULL) {
            throw std::runtime_error("detected a NULL pointer for a variable length string in '" + get_name(handle) + "'");
        }
        std::string output(vptr);
        return output;
    } else {
        size_t fixed_length = dtype.getSize();
        std::vector<char> buffer(fixed_length);
        handle.read(buffer.data(), dtype);
        return std::string(buffer.begin(), buffer.begin() + find_string_length(buffer.data(), fixed_length));
    }
}

/**
 * Load a 1-dimensional string dataset into a vector of strings.
 * @param handle Handle to the HDF5 scalar dataset.
 * @param full_length Length of the dataset as a 1-dimensional vector.
 * @param buffer_size Size of the buffer for holding loaded strings.
 * @return Vector of strings.
 */
inline std::vector<std::string> load_1d_string_dataset(const H5::DataSet& handle, hsize_t full_length, hsize_t buffer_size) {
    Stream1dStringDataset stream(&handle, full_length, buffer_size);
    std::vector<std::string> output;
    output.reserve(full_length);
    for (hsize_t i = 0; i < full_length; ++i, stream.next()) {
        output.emplace_back(stream.steal());
    }
    return output;
}

/**
 * Overload of `load_1d_string_dataset()` that determines the length via `get_1d_length()`.
 * @param handle Handle to the HDF5 scalar dataset.
 * @param buffer_size Size of the buffer for holding loaded strings.
 * @return Vector of strings.
 */
inline std::vector<std::string> load_1d_string_dataset(const H5::DataSet& handle, hsize_t buffer_size) {
    return load_1d_string_dataset(handle, get_1d_length(handle, false), buffer_size);
}

}

}

#endif
