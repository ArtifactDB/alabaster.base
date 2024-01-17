#ifndef RITSUKO_HDF5_STREAM_1D_NUMERIC_DATASET_HPP
#define RITSUKO_HDF5_STREAM_1D_NUMERIC_DATASET_HPP

#include "H5Cpp.h"

#include <vector>
#include <stdexcept>

#include "pick_1d_block_size.hpp"
#include "get_1d_length.hpp"
#include "get_name.hpp"
#include "as_numeric_datatype.hpp"

/**
 * @file Stream1dNumericDataset.hpp
 * @brief Stream a numeric 1-dimensional HDF5 dataset into memory.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * @brief Stream a numeric 1-dimensional HDF5 dataset into memory.
 * @tparam Type_ Type to represent the data in memory.
 *
 * This streams in a 1-dimensional HDF5 numeric dataset in contiguous blocks, using block sizes defined by `pick_1d_block_size()`.
 * Callers can then extract one value at a time or they can acquire the entire block.
 */
template<typename Type_>
class Stream1dNumericDataset {
public:
    /**
     * @param ptr Pointer to a HDF5 dataset handle.
     * @param length Length of the dataset as a 1-dimensional vector.
     * @param buffer_size Size of the buffer for holding streamed blocks of values.
     * Larger buffers improve speed at the cost of some memory efficiency.
     */
    Stream1dNumericDataset(const H5::DataSet* ptr, hsize_t length, hsize_t buffer_size) : 
        ptr(ptr), 
        full_length(length), 
        block_size(pick_1d_block_size(ptr->getCreatePlist(), full_length, buffer_size)),
        mspace(1, &block_size),
        dspace(1, &full_length),
        buffer(block_size)
    {}

    /**
     * Overloaded constructor where the length is automatically determined.
     *
     * @param ptr Pointer to a HDF5 dataset handle.
     * @param buffer_size Size of the buffer for holding streamed blocks of values.
     */
    Stream1dNumericDataset(const H5::DataSet* ptr, hsize_t buffer_size) : 
        Stream1dNumericDataset(ptr, get_1d_length(ptr->getSpace(), false), buffer_size) 
    {}

public:
    /**
     * @return Value at the current position of the stream.
     */
    Type_ get() {
        while (consumed >= available) {
            consumed -= available;
            load(); 
        }
        return buffer[consumed];
    }

    /**
     * @return Pair containing a pointer to and the length of an array.
     * The array holds all loaded values of the stream at its current position, up to the specified length.
     * Note that the pointer is only valid until the next invocation of `next()`.
     */
    std::pair<const Type_*, size_t> get_many() {
        while (consumed >= available) {
            consumed -= available;
            load();
        }
        return std::make_pair(buffer.data() + consumed, available - consumed);
    }

    /**
     * Advance the position of the stream by `jump`.
     *
     * @param jump Number of positions by which to advance the stream.
     */
    void next(size_t jump = 1) {
        consumed += jump;
    }

    /**
     * @return Length of the dataset.
     */
    hsize_t length() const {
        return full_length;
    }

    /**
     * @return Current position on the stream.
     */
    hsize_t position() const {
        return consumed + last_loaded;
    }

private:
    const H5::DataSet* ptr;
    hsize_t full_length, block_size;
    H5::DataSpace mspace;
    H5::DataSpace dspace;
    std::vector<Type_> buffer;

    hsize_t last_loaded = 0;
    hsize_t consumed = 0;
    hsize_t available = 0;

    void load() {
        if (last_loaded >= full_length) {
            throw std::runtime_error("requesting data beyond the end of the dataset at '" + get_name(*ptr) + "'");
        }
        available = std::min(full_length - last_loaded, block_size);
        constexpr hsize_t zero = 0;
        mspace.selectHyperslab(H5S_SELECT_SET, &available, &zero);
        dspace.selectHyperslab(H5S_SELECT_SET, &available, &last_loaded);
        ptr->read(buffer.data(), as_numeric_datatype<Type_>(), mspace, dspace);
        last_loaded += available;
    }
};

}

}

#endif
