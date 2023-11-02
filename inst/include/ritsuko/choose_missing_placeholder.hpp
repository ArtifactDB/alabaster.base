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
 * Choose an appropriate placeholder for missing values in an integer dataset.
 * This will try the various special values (the minimum, the maximum, and for signed types, 0)
 * before sorting the dataset and searching for an unused integer value.
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
    static_assert(std::numeric_limits<Type_>::is_integer);

    // Trying important points first; minima and maxima, and 0.
    if constexpr(std::numeric_limits<Type_>::is_signed) {
        for (int i = 0; i < 3; ++i) {
            Type_ candidate;
            if (i == 0) {
                candidate = std::numeric_limits<Type_>::min();
            } else if (i ==1) {
                candidate = std::numeric_limits<Type_>::max();
            } else {
                candidate = 0;
            }
            if (std::find(start, end, candidate) == end) {
                return std::make_pair(true, candidate);
            }
        }

    } else {
        for (int i = 0; i < 2; ++i) {
            Type_ candidate;
            if (i == 0) {
                candidate = std::numeric_limits<Type_>::max();
            } else {
                candidate = 0;
            }
            if (std::find(start, end, candidate) == end) {
                return std::make_pair(true, candidate);
            }
        }
    }

    // Well... going through it in order.
    std::set<Type_> uniq_sort(start, end);
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
 * Choose an appropriate placeholder for missing values in a floating-point dataset.
 * This will try the various IEEE special values (NaN, Inf, -Inf) and then some type-specific boundaries (the minimum, the maximum, and for signed types, 0)
 * before sorting the dataset and searching for an unused float. 
 *
 * @tparam Iterator_ Forward iterator for floating-point values.
 * @tparam Type_ Float type pointed to by `Iterator_`.
 *
 * @param start Start of the dataset.
 * @param end End of the dataset.
 * @param skip_nan Whether to skip NaN as a potential placeholder. 
 * Useful in frameworks like R that need special consideration of NaN payloads.
 *
 * @return Pair containing (i) a boolean indicating whether a placeholder was successfully found, and (ii) the chosen placeholder if the previous boolean is true.
 */
template<class Iterator, class Type_ = typename std::remove_cv<typename std::remove_reference<decltype(*(std::declval<Iterator>()))>::type>::type>
std::pair<bool, Type_> choose_missing_float_placeholder(Iterator start, Iterator end, bool skip_nan = false) {
    if constexpr(std::numeric_limits<Type_>::is_iec559) {
        if (!skip_nan) {
            bool has_nan = false;
            for (auto x = start; x != end; ++x) {
                if (std::isnan(*x)) {
                    has_nan = true;
                    break;
                }
            }
            if (!has_nan) {
                return std::make_pair(true, std::numeric_limits<Type_>::quiet_NaN());
            }
        }

        for (int i = 0; i < 2; ++i) {
            Type_ candidate = std::numeric_limits<Type_>::infinity() * (i == 0 ? 1 : -1);
            if (std::find(start, end, candidate) == end) {
                return std::make_pair(true, candidate);
            }
        }
    }

    // Trying important points first; minima and maxima, and 0.
    for (int i = 0; i < 3; ++i) {
        Type_ candidate;
        if (i == 0) {
            candidate = std::numeric_limits<Type_>::lowest();
        } else if (i ==1) {
            candidate = std::numeric_limits<Type_>::max();
        } else {
            candidate = 0;
        }
        if (std::find(start, end, candidate) == end) {
            return std::make_pair(true, candidate);
        }
    }

    // Well... going through it in order.
    std::set<Type_> uniq_sort(start, end);
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

}

#endif
