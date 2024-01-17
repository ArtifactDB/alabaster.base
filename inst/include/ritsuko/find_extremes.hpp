#ifndef RITSUKO_FIND_EXTREMES_HPP
#define RITSUKO_FIND_EXTREMES_HPP

#include "choose_missing_placeholder.hpp"
#include <limits>

/**
 * @file find_extremes.hpp
 * @brief Find extremes to use as missing placeholders.
 */

namespace ritsuko {

/**
 * @brief Extremes for integer types.
 */
struct IntegerExtremes {
    /**
     * Whether the lowest integer value is present.
     */
    bool has_lowest = false;

    /**
     * Whether the highest integer value is present.
     */
    bool has_highest = false;

    /**
     * Whether zero is present.
     */
    bool has_zero = false;
};

/**
 * Check for the presence of extreme values in an integer dataset.
 * This can be used to choose a missing placeholder value in an online fashion, by calling this function on blocks of the dataset;
 * if any of the extreme values are absent from all blocks, they can be used as the missing value placeholder.
 * By contrast, `choose_missing_integer_placeholder()` requires access to the full dataset.
 *
 * @tparam Iterator_ Forward iterator for integer values.
 * @tparam Mask_ Random access iterator for mask values.
 * @tparam Type_ Integer type pointed to by `Iterator_`.
 *
 * @param start Start of the dataset.
 * @param end End of the dataset.
 * @param mask Start of the mask vector. 
 * This should have the same length as `end - start`; each entry is true if the corresponding value of the integer dataset is masked, and false otherwise.
 *
 * @return Whether extreme values are present in `[start, end)`.
 * If `Type_` is unsigned, `IntegerExtremes::has_lowest` and `IntegerExtremes::has_zero` are the same.
 */
template<class Iterator, class Mask, class Type_ = typename std::remove_cv<typename std::remove_reference<decltype(*(std::declval<Iterator>()))>::type>::type>
IntegerExtremes find_integer_extremes(Iterator start, Iterator end, Mask mask) {
    static_assert(std::numeric_limits<Type_>::is_integer);

    IntegerExtremes output;
    output.has_zero = found(start, end, mask, 0);
    output.has_highest = found(start, end, mask, std::numeric_limits<Type_>::max());

    if constexpr(std::numeric_limits<Type_>::is_signed) {
        output.has_lowest = found(start, end, mask, std::numeric_limits<Type_>::min());
    } else {
        output.has_lowest = output.has_zero;
    }

    return output;
}

/**
 * Overload of `find_integer_extremes()` where no values are masked.
 *
 * @tparam Iterator_ Forward iterator for integer values.
 * @tparam Type_ Integer type pointed to by `Iterator_`.
 *
 * @param start Start of the dataset.
 * @param end End of the dataset.
 *
 * @return Whether extreme values are present in `[start, end)`.
 */
template<class Iterator, class Type_ = typename std::remove_cv<typename std::remove_reference<decltype(*(std::declval<Iterator>()))>::type>::type>
IntegerExtremes find_integer_extremes(Iterator start, Iterator end) {
    return find_integer_extremes(start, end, false);
}

/**
 * @brief Extremes for float types.
 */
struct FloatExtremes {
    /**
     * Whether any NaN value is present (no distinction is made for different NaN payloads).
     */
    bool has_nan = false;

    /**
     * Whether positive infinity is present.
     */
    bool has_positive_inf = false;

    /**
     * Whether negative infinity is present.
     */
    bool has_negative_inf = false;

    /**
     * Whether the lowest value of this type is present.
     * This corresponds to `std::numeric_limits<T>::lowest()`.
     */
    bool has_lowest = false;

    /**
     * Whether the highest value of this type is present.
     * This corresponds to `std::numeric_limits<T>::max()`.
     */
    bool has_highest = false;

    /**
     * Whether zero is present.
     */
    bool has_zero = false;
};

/**
 * Check for the presence of extreme values in a floating-point dataset.
 * This can be used to choose a missing placeholder value in an online fashion, by calling this function on blocks of the dataset;
 * if any of the extreme values are absent from all blocks, they can be used as the missing value placeholder.
 * By contrast, `choose_missing_float_placeholder()` requires access to the full dataset.
 *
 * @tparam Iterator_ Forward iterator for float values.
 * @tparam Mask_ Random access iterator for mask values.
 * @tparam Type_ Float type pointed to by `Iterator_`.
 *
 * @param start Start of the dataset.
 * @param end End of the dataset.
 * @param mask Start of the mask vector. 
 * This should have the same length as `end - start`; each entry is true if the corresponding value of the float dataset is masked, and false otherwise.
 * @param skip_nan Whether to skip searches for NaN.
 * Useful in frameworks like R that need special consideration of NaN payloads.
 *
 * @return Whether extreme values are present in `[start, end)`.
 * If `skip_nan = true`, `FloatExtremes::has_nan` is set to false and should be ignored.
 * If `Type_` is not an IEEE754-compliant float, users should ignore `FloatExtremes::has_nan`, `FloatExtremes::has_negative_inf` and `FloatExtremes::has_positive_inf`.
 */
template<class Iterator, class Mask, class Type_ = typename std::remove_cv<typename std::remove_reference<decltype(*(std::declval<Iterator>()))>::type>::type>
FloatExtremes find_float_extremes(Iterator start, Iterator end, Mask mask, bool skip_nan) {
    FloatExtremes output;

    if constexpr(std::numeric_limits<Type_>::is_iec559) {
        if (!skip_nan) {
            output.has_nan = check_for_nan(start, end, mask);
        }

        auto inf = std::numeric_limits<Type_>::infinity();
        output.has_positive_inf = found(start, end, mask, inf);
        output.has_negative_inf = found(start, end, mask, -inf);
    }

    output.has_lowest = found(start, end, mask, std::numeric_limits<Type_>::lowest());
    output.has_highest = found(start, end, mask, std::numeric_limits<Type_>::max());
    output.has_zero = found(start, end, mask, 0);

    return output;
}

/**
 * Overload of `find_float_extremes()` where no values are masked.
 *
 * @tparam Iterator_ Forward iterator for float values.
 * @tparam Type_ Float type pointed to by `Iterator_`.
 *
 * @param start Start of the dataset.
 * @param end End of the dataset.
 * @param skip_nan Whether to skip searches for NaN.
 * Useful in frameworks like R that need special consideration of NaN payloads.
 *
 * @return Whether extreme values are present in `[start, end)`.
 */
template<class Iterator, class Type_ = typename std::remove_cv<typename std::remove_reference<decltype(*(std::declval<Iterator>()))>::type>::type>
FloatExtremes find_float_extremes(Iterator start, Iterator end, bool skip_nan = false) {
    return find_float_extremes(start, end, false, skip_nan);
}

}

#endif
