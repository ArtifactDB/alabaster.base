#ifndef RITSUKO_HDF5_LOAD_DATASET_HPP
#define RITSUKO_HDF5_LOAD_DATASET_HPP

#include <string>
#include <vector>
#include <stdexcept>

#include "H5Cpp.h"

#include "get_name.hpp"
#include "Stream1dStringDataset.hpp"
#include "Stream1dNumericDataset.hpp"
#include "as_numeric_datatype.hpp"
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
 * @param handle Handle to the 1-dimensional HDF5 dataset.
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
 * @param handle Handle to the 1-dimensional HDF5 dataset.
 * @param buffer_size Size of the buffer for holding loaded strings.
 * @return Vector of strings.
 */
inline std::vector<std::string> load_1d_string_dataset(const H5::DataSet& handle, hsize_t buffer_size) {
    return load_1d_string_dataset(handle, get_1d_length(handle, false), buffer_size);
}


/**
 * Load a scalar numeric dataset into a single number.
 * @tparam Type_ Type of the number in memory.
 * @param handle Handle to the HDF5 scalar dataset.
 * @return Number containing the value of the sole dataset entry.
 */
template<typename Type_>
Type_ load_scalar_numeric_dataset(const H5::DataSet& handle) {
    Type_ output;
    handle.read(&output, as_numeric_datatype<Type_>());
    return output;
}

/**
 * Load a 1-dimensional numeric dataset into a vector.
 * @tparam Type_ Type of the number in memory.
 * @param handle Handle to the HDF5 dataset.
 * @param full_length Length of the dataset as a 1-dimensional vector.
 * @param buffer_size Size of the buffer for holding loaded strings.
 * @return Vector of numbers.
 */
template<typename Type_>
std::vector<Type_> load_1d_numeric_dataset(const H5::DataSet& handle, hsize_t full_length, hsize_t buffer_size) {
    Stream1dNumericDataset<Type_> stream(&handle, full_length, buffer_size);
    std::vector<Type_> output;
    output.reserve(full_length);
    for (hsize_t i = 0; i < full_length; ++i, stream.next()) {
        output.push_back(stream.get());
    }
    return output;
}

/**
 * Overload of `load_1d_numeric_dataset()` that determines the length via `get_1d_length()`.
 * @tparam Type_ Type of the number in memory.
 * @param handle Handle to the HDF5 dataset.
 * @param buffer_size Size of the buffer for holding loaded strings.
 * @return Vector of numbers.
 */
template<typename Type_>
std::vector<Type_> load_1d_numeric_dataset(const H5::DataSet& handle, hsize_t buffer_size) {
    return load_1d_numeric_dataset<Type_>(handle, get_1d_length(handle, false), buffer_size);
}

}

}

#endif
