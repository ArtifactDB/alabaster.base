#ifndef RITSUKO_ITERATE_ND_DATASET_HPP
#define RITSUKO_ITERATE_ND_DATASET_HPP

#include "H5Cpp.h"

#include <vector>
#include <algorithm>
#include <cmath>

/**
 * @file IterateNdDataset.hpp
 * @brief Iterate through an N-dimensional dataset by block.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * @brief Iterate through an N-dimensional dataset by block.
 *
 * This iterates through an N-dimensional dataset in a blockwise fashion, constructing `H5::DataSpace` objects to enable callers to easily read the dataset contents at each block.
 * Block sizes are typically determined from dataset chunking via `pick_nd_block_dimensions()`, which ensures efficient access of entire chunks at each step.
 */
struct IterateNdDataset {
    /**
     * @param d Dataset dimensions.
     * @param b Block dimensions, typically obtained from `pick_nd_block_dimensions()`.
     * This should be of the same length as `d`, where each value of `b` is no greater than its counterpart in `d`.
     */
    IterateNdDataset(std::vector<hsize_t> d, std::vector<hsize_t> b) : 
        data_extent(std::move(d)), 
        block_extent(std::move(b)), 
        ndims(data_extent.size()), 
        starts_internal(ndims), 
        counts_internal(block_extent), 
        dspace(ndims, data_extent.data()) 
    {
        for (auto b : block_extent) {
            total_size *= b;
        }

        if (total_size) {
            dspace.selectHyperslab(H5S_SELECT_SET, counts_internal.data(), starts_internal.data());
            mspace.setExtentSimple(ndims, counts_internal.data());
        } else {
            finished_internal = true;
        }
    }

    /**
     * Move to the next step in the iteration.
     * This will modify the state of all references returned by the getters.
     */
    void next() {
        // Attempting a shift from the last dimension as this is the fastest-changing.
        for (size_t i = ndims; i > 0; --i) {
            auto d = i - 1;
            starts_internal[d] += block_extent[d];

            // Shift was possible, breaking out.
            if (starts_internal[d] < data_extent[d]) {
                total_size /= counts_internal[d];
                counts_internal[d] = std::min(data_extent[d] - starts_internal[d], block_extent[d]);
                total_size *= counts_internal[d];
                break;
            }

            // Next step isn't possible as we've reached the end of the dataset.
            if (d == 0) {
                finished_internal = true;
                return;
            }

            // Reached the end of the current dimension extent; set it to zero, 
            // move to the next dimension and increment it.
            starts_internal[d] = 0;
            total_size /= counts_internal[d];
            counts_internal[d] = std::min(data_extent[d], block_extent[d]);
            total_size *= counts_internal[d];
        }

        dspace.selectHyperslab(H5S_SELECT_SET, counts_internal.data(), starts_internal.data());
        mspace.setExtentSimple(ndims, counts_internal.data());
    }

public:
    /**
     * @return Whether the iteration is finished.
     * All other getters should only be accessed if this is `true`.
     */
    bool finished() const {
        return finished_internal;
    }

    /**
     * @return Size of the current block, in terms of the number of elements.
     * This is usually equal to the product of the block dimensions used in the constructor,
     * except at the edges of the dataset where the current block may be truncated.
     */
    size_t current_block_size() const {
        return total_size;
    }

    /**
     * @return Starting coordinates of the current block. 
     */
    const std::vector<hsize_t>& starts () const {
        return starts_internal;
    }

    /**
     * @return Dimensions of the current block. 
     * This is usually equal to the block dimensions used in the constructor,
     * except at the edges of the dataset where the current block may be truncated.
     */
    const std::vector<hsize_t>& counts () const {
        return counts_internal;
    }

    /**
     * @return Dataspace for extracting block contents from file.
     */
    const H5::DataSpace& file_space() const {
        return dspace;
    }

    /**
     * @return Dataspace for storing the block contents in memory.
     * This assumes a contiguous memory allocation that has space for at least `total_size()` elements.
     */
    const H5::DataSpace& memory_space() const {
        return mspace;
    }

    /**
     * @return Dimensions of the dataset, as provided in the constructor.
     */
    const std::vector<hsize_t>& dimensions() const {
        return data_extent;
    }

    /**
     * @return Dimensions of the blocks, as provided in the constructor.
     */
    const std::vector<hsize_t>& block_dimensions() const {
        return block_extent;
    }

private:
    std::vector<hsize_t> data_extent, block_extent;
    size_t ndims;

    std::vector<hsize_t> starts_internal, counts_internal;
    H5::DataSpace mspace, dspace;
    bool finished_internal = false;
    size_t total_size = 1;
};

}

}

#endif
