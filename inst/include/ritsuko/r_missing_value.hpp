#ifndef RITSUKO_R_MISSING_HPP
#define RITSUKO_R_MISSING_HPP

#include <cstdint>
#include <cstring>

/**
 * @file r_missing_value.hpp
 * @brief Obtain R's missing value.
 */

namespace ritsuko {

/**
 * Create R's missing value for doubles, allowing us to mimic R's missingness concept in other languages.
 *
 * @return A quiet NaN with a payload of 1954, equivalent to R's double-precision missing value.
 */
inline double r_missing_value() {
    uint32_t tmp_value = 1;
    auto tmp_ptr = reinterpret_cast<unsigned char*>(&tmp_value);

    // Mimic R's generation of these values, but we can't use type punning as
    // this is not legal in C++, and we don't have bit_cast yet.
    double missing_value = 0;
    auto missing_ptr = reinterpret_cast<unsigned char*>(&missing_value);

    int step = 1;
    if (tmp_ptr[0] == 1) { // little-endian. 
        missing_ptr += sizeof(double) - 1;
        step = -1;
    }

    *missing_ptr = 0x7f;
    *(missing_ptr += step) = 0xf0;
    *(missing_ptr += step) = 0x00;
    *(missing_ptr += step) = 0x00;
    *(missing_ptr += step) = 0x00;
    *(missing_ptr += step) = 0x00;
    *(missing_ptr += step) = 0x07;
    *(missing_ptr += step) = 0xa2;

    return missing_value;
}

/**
 * Check for identical floating-point numbers, including NaN status and the payload.
 *
 * @tparam Float_ Floating-point type.
 *
 * @param x Pointer to a floating point value.
 * @param y Pointer to another floating point value.
 *
 * @return Whether or not `x` and `y` have identical bit patterns.
 */
template<typename Float_>
bool are_floats_identical(const Float_* x, const Float_* y) {
    auto xptr = reinterpret_cast<const unsigned char*>(x);
    auto yptr = reinterpret_cast<const unsigned char*>(y);
    return std::memcmp(xptr, yptr, sizeof(Float_)) == 0;
}

}

#endif
