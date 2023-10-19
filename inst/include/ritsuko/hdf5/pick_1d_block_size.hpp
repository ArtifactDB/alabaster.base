#ifndef RITSUKO_PICK_1D_BLOCK_SIZE_HPP
#define RITSUKO_PICK_1D_BLOCK_SIZE_HPP

#include "H5Cpp.h"

/**
 * @file pick_1d_block_size.hpp
 * @brief Pick a block size for a 1-dimensional HDF5 dataset.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Pick a block size to use for 1-dimensional iteration over a dataset.
 * For compressed datasets, this aims to be the smallest multiple of the chunk size that fits into a buffer.
 *
 * @param cplist The creation property list for this dataset.
 * @param full_length Length of this dataset, e.g., from `get_1d_length()`.
 * @param buffer_size Size of the buffer in terms of the number of elements.
 * Smaller values reduce peak memory usage at the cost of more iterations.
 *
 * @return The block size (in terms of the number of elements).
 */
inline hsize_t pick_1d_block_size(const H5::DSetCreatPropList& cplist, hsize_t full_length, hsize_t buffer_size = 10000) {
    if (full_length < buffer_size) {
        return full_length;
    }

    if (cplist.getLayout() != H5D_CHUNKED) {
        return buffer_size;
    }

    hsize_t chunk_size;
    cplist.getChunk(1, &chunk_size);

    // Finding the number of chunks to load per iteration; this is either
    // the largest multiple below 'buffer_size' or the chunk size.
    int num_chunks = (buffer_size / chunk_size);
    if (num_chunks == 0) {
        num_chunks = 1;
    }

    // This is already guaranteed to be less than 'full_length', as 
    // 'chunk_size <= full_length' from HDF5.
    return num_chunks * chunk_size;
}

}

}

#endif
