#ifndef RITSUKO_AS_NUMERIC_DATATYPE_HPP
#define RITSUKO_AS_NUMERIC_DATATYPE_HPP

#include <type_traits>
#include <cstdint>
#include "H5Cpp.h"

/**
 * @file as_numeric_datatype.hpp
 * @brief Choose a HDF5 datatype.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Choose the HDF5 datatype object corresponding to a particular C++ numeric type.
 * Currently, only fixed-width integer types (e.g., `uint16_t`, `int32_t`) and the usual floating-point types are supported.
 *
 * @tparam Type_ A numeric C++ type of fixed width.
 * This can be any of the fixed-width integers or a floating-point number of known precision.
 * @returns A HDF5 datatype object.
 */
template<typename Type_>
H5::PredType as_numeric_datatype() {
    if constexpr(std::is_same<Type_, uint8_t>::value) {
        return H5::PredType::NATIVE_UINT8;
    } else if constexpr(std::is_same<Type_, int8_t>::value) {
        return H5::PredType::NATIVE_INT8;
    } else if constexpr(std::is_same<Type_, uint16_t>::value) {
        return H5::PredType::NATIVE_UINT16;
    } else if constexpr(std::is_same<Type_, int16_t>::value) {
        return H5::PredType::NATIVE_INT16;
    } else if constexpr(std::is_same<Type_, uint32_t>::value) {
        return H5::PredType::NATIVE_UINT32;
    } else if constexpr(std::is_same<Type_, int32_t>::value) {
        return H5::PredType::NATIVE_INT32;
    } else if constexpr(std::is_same<Type_, uint64_t>::value) {
        return H5::PredType::NATIVE_UINT64;
    } else if constexpr(std::is_same<Type_, int64_t>::value) {
        return H5::PredType::NATIVE_INT64;
    } else if constexpr(std::is_same<Type_, float>::value) {
        return H5::PredType::NATIVE_FLOAT;
    } else {
        static_assert(std::is_same<Type_, double>::value, "specified type is not yet supported");
        return H5::PredType::NATIVE_DOUBLE;
    }
}

}

}

#endif
