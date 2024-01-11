#ifndef RITSUKO_IS_UTF8_STRING_HPP
#define RITSUKO_IS_UTF8_STRING_HPP

#include <type_traits>
#include <cstdint>
#include "H5Cpp.h"

/**
 * @file is_utf8_string.hpp
 * @brief Check if a string datatype uses a UTF-8 encoding.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Check if a HDF5 string datatype could represent strings that are not compatible with the UTF-8 encoding.
 *
 * Note that this returns `true` even if the string datatype uses ASCII encoding, given that ASCII is a subset of UTF-8.
 * As a result, this function is mostly performative as all valid HDF5 strings are encoded in either ASCII or UTF-8;
 * nonetheless, we run these checks to be explicit and to protect against the future addition of more encodings.
 *
 * @param stype HDF5 string datatype.
 * @return Whether `stype` uses UTF-8 (or ASCII) encoding.
 */
inline bool is_utf8_string(const H5::StrType& stype) {
    auto cset = stype.getCset();
    return (cset == H5T_CSET_ASCII || cset == H5T_CSET_UTF8);
}

/**
 * Overload of `is_utf8_string()` that accepts a HDF5 dataset handle.
 * @param handle Handle for a HDF5 dataset.
 * @return Whether the dataset holds strings that use UTF-8 (or ASCII) encoding.
 */
inline bool is_utf8_string(const H5::DataSet& handle) {
    if (handle.getTypeClass() != H5T_STRING) {
        return false;
    }
    return is_utf8_string(H5::StrType(handle));
}

/**
 * Overload of `is_utf8_string()` that accepts a HDF5 attribute handle.
 * @param handle Handle for a HDF5 attribute.
 * @return Whether the attribute holds strings that use UTF-8 (or ASCII) encoding.
 */
inline bool is_utf8_string(const H5::Attribute& handle) {
    if (handle.getTypeClass() != H5T_STRING) {
        return false;
    }
    return is_utf8_string(handle.getStrType());
}

}

}

#endif
