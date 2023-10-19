#ifndef RITSUKO_HDF5_FORBID_LARGE_INTEGERS_HPP
#define RITSUKO_HDF5_FORBID_LARGE_INTEGERS_HPP

#include "H5Cpp.h"
#include <stdexcept>

/**
 * @file forbid_large_integers.hpp
 * @brief Forbid large integers in HDF5 datasets.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Forbid the existence of integer types that can hold values greater than a `bit_limit`-bit signed integer.
 * If a dataset with a large integer type is detected, an error is raised.
 *
 * @param handle Integer dataset.
 * @param bit_limit Limit on the size of the integer.
 * @param name String containing the name of the dataset for error messages.
 */
inline void forbid_large_integers(const H5::DataSet& handle, size_t bit_limit, const char* name) {
    H5::IntType itype(handle);

    bool failed = false;
    if (itype.getSign() == H5T_SGN_NONE) {
        if (itype.getPrecision() >= bit_limit) {
            failed = true;
        }
    } else if (itype.getPrecision() > bit_limit) {
        failed = true;
    }

    if (failed) {
        throw std::runtime_error("data type exceeds the range of a " + std::to_string(bit_limit) + "-bit signed integer for '" + std::string(name) + "'");
    }
}

}

}

#endif
