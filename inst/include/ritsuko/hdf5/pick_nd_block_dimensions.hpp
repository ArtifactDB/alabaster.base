#ifndef RITSUKO_PICK_ND_BLOCK_DIMENSIONS_HPP
#define RITSUKO_PICK_ND_BLOCK_DIMENSIONS_HPP

#include "H5Cpp.h"

/**
 * @file pick_nd_block_dimensions.hpp
 * @brief Pick block dimensions for an N-dimensional HDF5 dataset.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Pick block dimensions to use for iteration over an N-dimensional dataset.
 * For compressed datasets, this aims to be the smallest multiple of the chunk size that fits into a buffer.
 *
 * @param cplist The creation property list for this dataset.
 * @param dimensions Dimensions of this dataset.
 * @param buffer_size Size of the buffer in terms of the number of elements.
 * Smaller values reduce peak memory usage at the cost of more iterations.
 *
 * @return The block dimensions. 
 */
inline std::vector<hsize_t> pick_nd_block_dimensions(const H5::DSetCreatPropList& cplist, const std::vector<hsize_t>& dimensions, hsize_t buffer_size = 10000) {
    size_t ndims = dimensions.size();
    std::vector<hsize_t> chunk_extent(ndims, 1);
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
        block_extent[d] = std::min(block_extent[d], dimensions[d]); // should be a no-op, but we do this just in case.
        block_size *= block_extent[d];
    }

    if (block_size) {
        for (hsize_t i = ndims; i > 0; --i) {
            int multiple = buffer_size / block_size;
            if (multiple <= 1) {
                break;
            }
            auto d = i - 1;
            block_size /= block_extent[d];
            block_extent[d] = std::min(dimensions[d], block_extent[d] * multiple);
            block_size *= block_extent[d];
        }
    } else {
        std::fill(block_extent.begin(), block_extent.end(), 0);
    }

    return block_extent;
}

}

}

#endif
