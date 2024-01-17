#ifndef RITSUKO_HDF5_STREAM_1D_STRING_DATASET_HPP
#define RITSUKO_HDF5_STREAM_1D_STRING_DATASET_HPP

#include "H5Cpp.h"

#include <vector>
#include <string>
#include <stdexcept>

#include "pick_1d_block_size.hpp"
#include "get_1d_length.hpp"
#include "get_name.hpp"
#include "as_numeric_datatype.hpp"
#include "_strings.hpp"

/**
 * @file Stream1dStringDataset.hpp
 * @brief Stream a numeric 1-dimensional HDF5 dataset into memory.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * @brief Stream a 1-dimensional HDF5 string dataset into memory.
 *
 * This streams in a 1-dimensional HDF5 string dataset in contiguous blocks, using block sizes defined by `pick_1d_block_size()`.
 * Callers can then iterate over the individual strings.
 */
class Stream1dStringDataset {
public:
    /**
     * @param ptr Pointer to a HDF5 dataset handle.
     * @param length Length of the dataset as a 1-dimensional vector.
     * @param buffer_size Size of the buffer for holding streamed blocks of values.
     * Larger buffers improve speed at the cost of some memory efficiency.
     */
    Stream1dStringDataset(const H5::DataSet* ptr, hsize_t length, hsize_t buffer_size) : 
        ptr(ptr), 
        full_length(length), 
        block_size(pick_1d_block_size(ptr->getCreatePlist(), full_length, buffer_size)),
        mspace(1, &block_size),
        dspace(1, &full_length),
        dtype(ptr->getDataType()),
        is_variable(dtype.isVariableStr())
    {
        if (is_variable) {
            var_buffer.resize(block_size);
        } else {
            fixed_length = dtype.getSize();
            fix_buffer.resize(fixed_length * block_size);
        }
        final_buffer.resize(block_size);
    }

    /**
     * Overloaded constructor where the length is automatically determined.
     *
     * @param ptr Pointer to a HDF5 dataset handle.
     * @param buffer_size Size of the buffer for holding streamed blocks of values.
     */
    Stream1dStringDataset(const H5::DataSet* ptr, hsize_t buffer_size) : 
        Stream1dStringDataset(ptr, get_1d_length(ptr->getSpace(), false), buffer_size) 
    {}

public:
    /**
     * @return String at the current position of the stream.
     */
    std::string get() {
        while (consumed >= available) {
            consumed -= available;
            load(); 
        }
        return final_buffer[consumed];
    }

    /**
     * @return String at the current position of the stream.
     * Unlike `get()`, this avoids a copy by directly acquiring the string,
     * but it invalidates all subsequent `get()` and `steal()` requests until `next()` is called.
     */
    std::string steal() {
        while (consumed >= available) {
            consumed -= available;
            load(); 
        }
        return std::move(final_buffer[consumed]);
    }

    /**
     * Advance to the next position of the stream.
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

    H5::DataType dtype;
    bool is_variable;
    std::vector<char*> var_buffer;
    size_t fixed_length = 0;
    std::vector<char> fix_buffer;
    std::vector<std::string> final_buffer;

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

        if (is_variable) {
            ptr->read(var_buffer.data(), dtype, mspace, dspace);
            [[maybe_unused]] VariableStringCleaner deletor(dtype.getId(), mspace.getId(), var_buffer.data());
            for (hsize_t i = 0; i < available; ++i) {
                if (var_buffer[i] == NULL) {
                    throw std::runtime_error("detected a NULL pointer for a variable length string in '" + get_name(*ptr) + "'");
                }
                auto& curstr = final_buffer[i];
                curstr.clear();
                curstr.insert(0, var_buffer[i]);
            }

        } else {
            auto bptr = fix_buffer.data();
            ptr->read(bptr, dtype, mspace, dspace);
            for (size_t i = 0; i < available; ++i, bptr += fixed_length) {
                auto& curstr = final_buffer[i];
                curstr.clear();
                curstr.insert(curstr.end(), bptr, bptr + find_string_length(bptr, fixed_length));
            }
        }

        last_loaded += available;
    }
};

}

}

#endif
