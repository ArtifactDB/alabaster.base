#ifndef RITSUKO_HDF5_LOAD_1D_STRING_DATASET_HPP
#define RITSUKO_HDF5_LOAD_1D_STRING_DATASET_HPP

#include "H5Cpp.h"
#include <vector>
#include <cstring>

#include "pick_1d_block_size.hpp"
#include "iterate_1d_blocks.hpp"

/**
 * @file load_1d_string_dataset.hpp
 * @brief Load and iterate over a 1-dimensional HDF5 string dataset.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Iterate across a string dataset, extracting each string and running a user-specified function.
 * This works for both variable- and fixed-length strings, and performs iteration via `iterate_1d_blocks()` to avoid loading everything into memory at once.
 *
 * @tparam Function_ Function class that accepts `(hsize_t i, const char* start, size_t len)`
 * where `i` is the index of the string from `[start, start + len)`.
 *
 * @param handle Handle to a string dataset.
 * @param full_length Length of the dataset in `handle`, usually obtained by `get_1d_length()`.
 * @param buffer_size Buffer size to use for iteration in `iterate_1d_blocks()`.
 * @param fun Function to be called on each string.
 * It can be assumed that the consecutive calls to `fun` will operate on consecutive `i`.
 */
template<class Function_>
void load_1d_string_dataset(const H5::DataSet& handle, hsize_t full_length, hsize_t buffer_size, Function_ fun) {
    auto block_size = pick_1d_block_size(handle.getCreatePlist(), full_length, buffer_size);
    auto dtype = handle.getDataType();

    if (dtype.isVariableStr()) {
        std::vector<char*> buffer(block_size);
        iterate_1d_blocks(
            full_length, 
            block_size, 
            [&](hsize_t start, hsize_t len, const H5::DataSpace& mspace, const H5::DataSpace& dspace) -> void {
                handle.read(buffer.data(), dtype, mspace, dspace);
                for (hsize_t i = 0; i < len; ++i) {
                    fun(start + i, buffer[i], std::strlen(buffer[i]));
                }
                H5Dvlen_reclaim(dtype.getId(), mspace.getId(), H5P_DEFAULT, buffer.data());
            }
        );

    } else {
        size_t len = dtype.getSize();
        std::vector<char> buffer(len * block_size);
        iterate_1d_blocks(
            full_length, 
            block_size, 
            [&](hsize_t start, hsize_t length, const H5::DataSpace& mspace, const H5::DataSpace& dspace) -> void {
                handle.read(buffer.data(), dtype, mspace, dspace);
                auto ptr = buffer.data();
                for (size_t i = 0; i < length; ++i, ptr += len) {
                    size_t j = 0;
                    for (; j < len && ptr[j] != '\0'; ++j) {}
                    fun(start + i, ptr, j);
                }
            }
        );
    }
}

}

}

#endif

