#ifndef RITSUKO_HDF5_ITERATE_1D_BLOCKS_HPP
#define RITSUKO_HDF5_ITERATE_1D_BLOCKS_HPP

#include "H5Cpp.h"
#include <algorithm>

/**
 * @file iterate_1d_blocks.hpp
 * @brief Blockwise iteration through a 1-dimensional HDF5 dataset.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Iterate through a 1-dimensional dataset via contiguous blocks.
 *
 * @param full_length Length of the dataset, usually obtained from `get_1d_length()`.
 * @param block_size Size of the blocks, usually calculated by `pick_1d_block_size()`.
 * @param fun Function that accepts `(hsize_t start, hsize_t len, H5::DataSpace& memspace, H5::DataSpace& dataspace)` and is called on each block.
 * In each call, the block contains elements from `[start, start + len)`.
 * `dataspace` is configured to extract that block from the dataset, while `memspace` is configured to deposit the block contents in a buffer from `[0, len)`.
 * It can be assumed that consecutive calls to `fun` will operate on consecutive contiguous blocks.
 */
template<class Function_>
void iterate_1d_blocks(hsize_t full_length, hsize_t block_size, Function_ fun) {
    H5::DataSpace mspace(1, &block_size);
    H5::DataSpace dspace(1, &full_length);
    hsize_t start = 0;

    for (hsize_t counter = 0; counter < full_length; counter += block_size) {
        hsize_t limit = std::min(full_length - counter, block_size);
        mspace.selectHyperslab(H5S_SELECT_SET, &limit, &start);
        dspace.selectHyperslab(H5S_SELECT_SET, &limit, &counter);
        fun(counter, limit, mspace, dspace);
    }
}

}

}

#endif
