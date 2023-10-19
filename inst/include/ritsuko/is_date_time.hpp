#ifndef RITSUKO_IS_DATE_TIME_HPP
#define RITSUKO_IS_DATE_TIME_HPP

#include <cctype>
#include <cstdlib>
#include <iostream>

/**
 * @file is_date_time.hpp
 * @brief Utilities to check date and time formats.
 */

namespace ritsuko {

/**
 * Does a string start with a XXXX-YY-ZZ date, for approximately valid combinations of YY and ZZ?
 * (This is only approximate as we do not check the exact correctness of the number of days for each month.)
 *
 * @param[in] ptr Pointer to a C-style string containing at least 10 characters.
 *
 * @return Whether or not the string starts with a date.
 */
inline bool is_date_prefix(const char* ptr) {
    for (size_t p = 0; p < 4; ++p) {
        if (!std::isdigit(ptr[p])) {
            return false;
        }
    }

    if (ptr[4] != '-') {
        return false;
    }

    for (size_t p = 5; p <= 6; ++p) {
        if (!std::isdigit(ptr[p])) {
            return false;
        }
    }
    if (ptr[5] == '1') {
        if (ptr[6] > '2') {
            return false;
        }
    } else if (ptr[5] != '0') {
        return false;
    }

    if (ptr[7] != '-') {
        return false;
    }

    for (size_t p = 8; p <= 9; ++p) {
        if (!std::isdigit(ptr[p])) {
            return false;
        }
    }
    if (ptr[8] == '3') {
        if (ptr[9] > '1') {
            return false;
        }
    } else if (ptr[8] > '3') {
        return false;
    }

    return true;
}

/**
 * @param[in] ptr Pointer to a C-style string.
 * @param len Length of the string referenced by `ptr`, excluding the null terminator.
 *
 * @return Whether `ptr` refers to a XXXX-YY-ZZ date, for approximately valid combinations of YY and ZZ (see `is_date_prefix()` for details).
 */
inline bool is_date(const char* ptr, size_t len) {
    if (len != 10) {
        return false;
    }
    return is_date_prefix(ptr);
}

/**
 * @cond
 */
inline bool okay_hours(const char* ptr, size_t offset) {
    for (size_t i = 0; i < 2; ++i) {
        if (!std::isdigit(ptr[offset + i])) {
            return false;
        }
    }

    if (ptr[offset] == '2') {
        if (ptr[offset + 1] > '4') {
            return false;
        }
    } else if (ptr[offset] > '2') {
        return false;
    }

    return true;
}

inline bool okay_minutes(const char* ptr, size_t offset) {
    for (size_t i = 0; i < 2; ++i) {
        if (!std::isdigit(ptr[offset + i])) {
            return false;
        }
    }

    if (ptr[offset] > '5') {
        return false;
    }

    return true;
}

inline bool okay_seconds(const char* ptr, size_t offset) {
    for (size_t i = 0; i < 2; ++i) {
        if (!std::isdigit(ptr[offset + i])) {
            return false;
        }
    }

    if (ptr[offset] == '6') { // allow for leap seconds.
        if (ptr[offset + 1] > '0') {
            return false;
        }
    } else if (ptr[offset] > '5') {
        return false;
    }

    return true;
}
/**
 * @endcond
 */

/**
 * Does a string finish with an RFC3339-compliant timestamp, i.e., does the substring starting at `T` after the date follow the RFC3339 specification?
 * Note that the timestamp validity checks are only approximate as the correctness of leap seconds are not currently considered.
 * It is expected that the start of the string up to the `T` was already validated with `is_date_prefix()`.
 *
 * @param[in] ptr Pointer to a character array containing at least 10 characters.
 * This should start from the 10th position in the original string, i.e., `T` in the timestamp.
 * @param len Length of the string in `ptr`.
 *
 * @return Whether or not the string finishes with an RFC3339-compliant timestamp.
 */
inline bool is_rfc3339_suffix(const char* ptr, size_t len) {
    if (ptr[0] != 'T') {
        return false;
    }

    // Checking the time.
    if (!okay_hours(ptr, 1)) {
        return false;
    }
    if (ptr[3] != ':') {
        return false;
    }
    if (!okay_minutes(ptr, 4)) {
        return false;
    }
    if (ptr[6] != ':') {
        return false;
    }
    if (!okay_seconds(ptr, 7)) {
        return false;
    }

    size_t shift = 0;
    bool zero_fraction = true;
    if (ptr[9] == '.') { // handling fractional seconds; must have one digit.
        constexpr size_t start = 10;
        size_t counter = start;
        while (counter < len && std::isdigit(ptr[counter])) {
            if (ptr[counter] != '0') {
                zero_fraction = false;
            }
            ++counter;
        }
        if (counter == start) {
            return false; 
        }
        shift = counter - start + 1; // +1 to account for the period.
    }

    // Checking special case of 24:00:00.
    if (ptr[1] == '2' && ptr[2] == '4') { 
        if (ptr[4] != '0' || ptr[5] != '0' || ptr[7] != '0' || ptr[8] != '0' || !zero_fraction) {
            return false;
        }
    }

    // Checking special case of leap years.
    if (ptr[7] == '6' && ptr[8] == '0') { 
        if (!zero_fraction) {
            return false;
        }
    }

    // Now checking the timezone.
    size_t tz_start = 9 + shift;
    if (tz_start >= len) {
        return false;
    }
    if (ptr[tz_start] == 'Z') {
        return (len == tz_start + 1);
    } 

    if (len != tz_start + 6) {
        return false;
    }
    if (ptr[tz_start] != '+' && ptr[tz_start] != '-') {
        return false;
    }
    if (!okay_hours(ptr, tz_start + 1)) {
        return false;
    }
    if (ptr[tz_start + 3] != ':') {
        return false;
    }
    if (!okay_minutes(ptr, tz_start + 4)) {
        return false;
    }

    return true;
}

/**
 * Does a string follow the RFC3339 format?
 * This uses `is_date_prefix()` and `is_rfc3339_suffix()` to check the date and the rest of the timestamp, respectively.
 *
 * @param[in] ptr Pointer to a C-style string. 
 * @param len Length of the string in `ptr`.
 *
 * @return Whether or not the string is RFC3339-compliant.
 */
inline bool is_rfc3339(const char* ptr, size_t len) {
    if (len < 20) { // YYYY-MM-DDTHH:MM:SSZ is the shortest format.
        return false;
    } 

    if (!is_date_prefix(ptr)) {
        return false;
    }

    // Skip the characters associated with the date.
    return is_rfc3339_suffix(ptr + 10, len - 10);
}

}

#endif
