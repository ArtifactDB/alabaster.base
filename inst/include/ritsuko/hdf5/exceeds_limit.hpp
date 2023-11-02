#ifndef RITSUKO_HDF5_FORBID_LARGE_INTEGERS_HPP
#define RITSUKO_HDF5_FORBID_LARGE_INTEGERS_HPP

#include "H5Cpp.h"
#include <stdexcept>

/**
 * @file exceeds_limit.hpp
 * @brief Check for larger-than-expected types in HDF5 datasets.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Check if a HDF5 dataset has a type that might hold values beyond the range of a limiting integer type. 
 * This is used by validators to ensure that a dataset can be represented in memory by the limiting type.
 *
 * @param handle Handle for a HDF5 dataset.
 * @param precision Number of bits in the limiting integer type, assuming 2's complement.
 * @param is_signed Whether the limiting integer type is signed.
 *
 * @return Whether the dataset uses a datatype than cannot be represented by the limiting integer type.
 * `true` is also returned for non-integer datasets.
 */
inline bool exceeds_integer_limit(const H5::DataSet& handle, size_t precision, bool is_signed) {
    if (handle.getTypeClass() != H5T_INTEGER) {
        return true;
    }

    H5::IntType itype(handle);
    if (itype.getSign() == H5T_SGN_NONE) {
        if (is_signed) {
            return (itype.getPrecision() >= precision); // equality, as one bit of the limiting type is used for the sign.
        } else {
            return (itype.getPrecision() > precision);
        }
    } else {
        if (is_signed) {
            return (itype.getPrecision() > precision);
        } else {
            return true;
        }
    }
}

/**
 * Check if a HDF5 dataset has a type that might hold values beyond the range of a limiting float type. 
 * This is used by validators to ensure that a dataset can be represented in memory by the limiting type.
 *
 * @param handle Handle for a HDF5 dataset.
 * @param precision Number of bits in the limiting float type, assuming IEEE754 floats.
 *
 * @return Whether the dataset uses a datatype than cannot be represented by the limiting float type.
 * `true` is also returned for non-numeric datasets.
 */
inline bool exceeds_float_limit(const H5::DataSet& handle, size_t precision) {
    auto tclass = handle.getTypeClass();
    if (tclass == H5T_INTEGER) {
        if (precision >= 64) {
            return exceeds_integer_limit(handle, 52, true);
        } else if (precision >= 32) {
            return exceeds_integer_limit(handle, 24, true);
        } else {
            return true;
        }
    } else if (tclass == H5T_FLOAT) {
        H5::FloatType ftype(handle);
        return ftype.getPrecision() > precision;
    } else {
        return true;
    }
}

}

}

#endif
