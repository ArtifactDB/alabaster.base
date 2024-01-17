#ifndef RITSUKO_CHOOSE_MISSING_PLACEHOLDER_HPP
#define RITSUKO_CHOOSE_MISSING_PLACEHOLDER_HPP

#include <limits>
#include <set>
#include <type_traits>
#include <cmath>
#include <algorithm>

/**
 * @file choose_missing_placeholder.hpp
 * @brief Choose a placeholder for missing values.
 */

namespace ritsuko {

/**
 * @cond
 */
template<class Iterator, class Mask, class Type>
bool found(Iterator start, Iterator end, Mask mask, Type candidate) {
    if constexpr(std::is_same<Mask, bool>::value) {
        return (std::find(start, end, candidate) != end);
    } else {
        for (; start != end; ++start, ++mask) {
            if (!*mask && candidate == *start) {
                return true;
            }
        }
        return false;
    }
}

template<class Iterator, class Mask, class Type = typename std::remove_cv<typename std::remove_reference<decltype(*(std::declval<Iterator>()))>::type>::type>
std::set<Type> create_unique_set(Iterator start, Iterator end, Mask mask) {
    if constexpr(std::is_same<Mask, bool>::value) {
        return std::set<Type>(start, end);
    } else {
        std::set<Type> output;
        for (; start != end; ++start, ++mask) {
            if (!*mask) {
                output.insert(*start);
            }
        }
        return output;
    }
}

template<class Iterator, class Mask, class Type = typename std::remove_cv<typename std::remove_reference<decltype(*(std::declval<Iterator>()))>::type>::type>
bool check_for_nan(Iterator start, Iterator end, Mask mask) {
    if constexpr(std::is_same<Mask, bool>::value) {
        for (auto x = start; x != end; ++x) {
            if (std::isnan(*x)) {
                return true;
            }
        }
    } else {
        auto sIt = mask;
        for (auto x = start; x != end; ++x, ++sIt) {
            if (!*sIt && std::isnan(*x)) {
                return true;
            }
        }
    }

    return false;
}
/**
 * @endcond
 */

/**
 * Choose an appropriate placeholder for missing values in an integer dataset, after ignoring all the masked values.
 * This will try the various special values (the minimum, the maximum, and for signed types, 0)
 * before sorting the dataset and searching for an unused integer value.
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
 * @return Pair containing (i) a boolean indicating whether a placeholder was successfully found, and (ii) the chosen placeholder if the previous boolean is true.
 */
template<class Iterator, class Mask, class Type_ = typename std::remove_cv<typename std::remove_reference<decltype(*(std::declval<Iterator>()))>::type>::type>
std::pair<bool, Type_> choose_missing_integer_placeholder(Iterator start, Iterator end, Mask mask) {
    static_assert(std::numeric_limits<Type_>::is_integer);

    // Trying important points first; minima and maxima, and 0.
    if constexpr(std::numeric_limits<Type_>::is_signed) {
        auto candidate = std::numeric_limits<Type_>::min();
        if (!found(start, end, mask, candidate)) {
            return std::make_pair(true, candidate);
        }
    }

    {
        auto candidate = std::numeric_limits<Type_>::max();
        if (!found(start, end, mask, candidate)) {
            return std::make_pair(true, candidate);
        }
    }

    if (!found(start, end, mask, 0)) {
        return std::make_pair(true, 0);
    }

    // Well... going through it in order.
    auto uniq_sort = create_unique_set(start, end, mask);
    Type_ last = std::numeric_limits<Type_>::min();
    for (auto x : uniq_sort) {
        if (last + 1 < x) {
            return std::make_pair(true, last + 1);
        }
        last = x;
    }

    return std::make_pair(false, 0);
}

/**
 * Overload of `choose_missing_integer_placeholder()` where no values are masked.
 *
 * @tparam Iterator_ Forward iterator for integer values.
 * @tparam Type_ Integer type pointed to by `Iterator_`.
 *
 * @param start Start of the dataset.
 * @param end End of the dataset.
 *
 * @return Pair containing (i) a boolean indicating whether a placeholder was successfully found, and (ii) the chosen placeholder if the previous boolean is true.
 */
template<class Iterator, class Type_ = typename std::remove_cv<typename std::remove_reference<decltype(*(std::declval<Iterator>()))>::type>::type>
std::pair<bool, Type_> choose_missing_integer_placeholder(Iterator start, Iterator end) {
    return choose_missing_integer_placeholder(start, end, false);
}

/**
 * Choose an appropriate placeholder for missing values in a floating-point dataset, after ignoring all masked values.
 * This will try the various IEEE special values (NaN, Inf, -Inf) and then some type-specific boundaries (the minimum, the maximum, and for signed types, 0)
 * before sorting the dataset and searching for an unused float. 
 *
 * @tparam Iterator_ Forward iterator for floating-point values.
 * @tparam Type_ Float type pointed to by `Iterator_`.
 *
 * @param start Start of the dataset.
 * @param end End of the dataset.
 * @param mask Start of the mask vector.
 * @param skip_nan Whether to skip NaN as a potential placeholder. 
 * Useful in frameworks like R that need special consideration of NaN payloads.
 *
 * @return Pair containing (i) a boolean indicating whether a placeholder was successfully found, and (ii) the chosen placeholder if the previous boolean is true.
 */
template<class Iterator, class Mask, class Type_ = typename std::remove_cv<typename std::remove_reference<decltype(*(std::declval<Iterator>()))>::type>::type>
std::pair<bool, Type_> choose_missing_float_placeholder(Iterator start, Iterator end, Mask mask, bool skip_nan) {
    if constexpr(std::numeric_limits<Type_>::is_iec559) {
        if (!skip_nan) {
            if (!check_for_nan(start, end, mask)) {
                return std::make_pair(true, std::numeric_limits<Type_>::quiet_NaN());
            }
        }

        // Trying positive and negative Infs.
        auto inf = std::numeric_limits<Type_>::infinity();
        if (!found(start, end, mask, inf)) {
            return std::make_pair(true, inf);
        }

        auto ninf = -inf;
        if (!found(start, end, mask, ninf)) {
            return std::make_pair(true, ninf);
        }
    }

    // Trying important points first; minima and maxima, and 0.
    {
        auto candidate = std::numeric_limits<Type_>::lowest();
        if (!found(start, end, mask, candidate)) {
            return std::make_pair(true, candidate);
        }
    }

    {
        auto candidate = std::numeric_limits<Type_>::max();
        if (!found(start, end, mask, candidate)) {
            return std::make_pair(true, candidate);
        }
    }

    if (!found(start, end, mask, 0)) {
        return std::make_pair(true, 0);
    }

    // Well... going through it in order.
    auto uniq_sort = create_unique_set(start, end, mask);
    Type_ last = std::numeric_limits<Type_>::lowest();
    for (auto x : uniq_sort) {
        if (std::isfinite(x)) {
            Type_ candidate = last + (x - last) / 2;
            if (candidate != last && candidate != x) {
                return std::make_pair(true, candidate);
            }
            last = x;
        }
    }

    return std::make_pair(false, 0);
}

/**
 * Overload of `choose_missing_float_placeholder()` where no values are masked.
 *
 * @tparam Iterator_ Forward iterator for floating-point values.
 * @tparam Type_ Integer type pointed to by `Iterator_`.
 *
 * @param start Start of the dataset.
 * @param end End of the dataset.
 * @param skip_nan Whether to skip NaN as a potential placeholder. 
 *
 * @return Pair containing (i) a boolean indicating whether a placeholder was successfully found, and (ii) the chosen placeholder if the previous boolean is true.
 */
template<class Iterator, class Type_ = typename std::remove_cv<typename std::remove_reference<decltype(*(std::declval<Iterator>()))>::type>::type>
std::pair<bool, Type_> choose_missing_float_placeholder(Iterator start, Iterator end, bool skip_nan = false) {
    return choose_missing_float_placeholder(start, end, false, skip_nan);
}

}

#endif
