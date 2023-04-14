#ifndef UZUKI2_UTILS_HPP
#define UZUKI2_UTILS_HPP

#include <cctype>
#include <string>
#include <algorithm>

namespace uzuki2 {

inline bool is_date_prefix(const std::string& val) {
    for (size_t p = 0; p < 10; ++p) {
        if (p == 4 || p == 7) {
            if (val[p] != '-') {
                return false;
            }
        } else {
            if (!std::isdigit(val[p])) {
                return false;
            }
        }
    }

    if (val[5] == '1') {
        if (val[6] > '2') {
            return false;
        }
    } else if (val[5] != '0') {
        return false;
    }

    if (val[8] == '3') {
        if (val[9] > '1') {
            return false;
        }
    } else if (val[8] > '3') {
        return false;
    }

    return true;
}

inline bool is_date(const std::string& val) {
    if (val.size() != 10) {
        return false;
    } 
    return is_date_prefix(val);
}

inline bool okay_hours(const std::string& val, size_t offset) {
    if (val[offset] == '2') {
        if (val[offset + 1] > '4') {
            return false;
        }
    } else if (val[offset] > '2') {
        return false;
    }
    return true;
}

inline bool okay_minutes(const std::string& val, size_t offset) {
    if (val[offset] > '5') {
        return false;
    }
    return true;
}

inline bool is_rfc3339(const std::string& val) {
    if (val.size() < 20) { // YYYY-MM-DDTHH:MM:SSZ is the shortest format.
        return false;
    } 

    if (!is_date_prefix(val)) {
        return false;
    }

    if (val[10] != 'T') {
        return false;
    }

    // Checking the time.
    for (size_t p = 11; p < 19; ++p) {
        if (p == 13 || p == 16) {
            if (val[p] != ':') {
                return false;
            }
        } else {
            if (!std::isdigit(val[p])) {
                return false;
            }
        }
    }

    if (!okay_hours(val, 11)) {
        return false;
    }

    bool all_zero = true;
    for (size_t i = 0; i < 2; ++i) {
        size_t offset = 14 + i * 3;
        if (!okay_minutes(val, offset)) {
            return false;
        }
        if (val[offset] != '0' || val[offset + 1] != '0') {
            all_zero = false;
        }
    }

    size_t shift = 0;
    if (val[19] == '.') { // handling fractional seconds; must have one digit.
        size_t start = 20;
        size_t counter = start;
        while (counter < val.size() && std::isdigit(val[counter])) {
            if (val[counter] != '0') {
                all_zero = false;
            }
            ++counter;
        }
        if (counter == start) {
            return false; 
        }
        shift = counter - start + 1; // +1 to account for the period.
    }

    if (!all_zero && val[11] == '2' && val[12] == '4') { // checking special case of 24:00:00.
        return false;
    }

    // Now checking the timezone.
    size_t tz_start = 19 + shift;
    if (val[tz_start] == 'Z') {
        return (val.size() == tz_start + 1);
    } 

    if (val[tz_start] != '+' && val[tz_start] != '-') {
        return false;
    }

    if (val.size() != tz_start + 6) {
        return false;
    }

    for (size_t p = 1; p < 6; ++p) {
        if (p == 3) {
            if (val[tz_start + p] != ':') {
                return false;
            }
        } else {
            if (!std::isdigit(val[tz_start + p])) {
                return false;
            }
        }
    }

    if (!okay_hours(val, tz_start + 1)) {
        return false;
    }

    if (!okay_minutes(val, tz_start + 4)) {
        return false;
    }

    return true;
}

template<class CustomExternals>
struct ExternalTracker {
    ExternalTracker(CustomExternals e) : getter(std::move(e)) {}

    void* get(size_t i) {
        indices.push_back(i);
        return getter.get(i);
    };

    size_t size() const {
        return getter.size();
    }

    CustomExternals getter;
    std::vector<size_t> indices;

    void validate() {
        // Checking that the external indices match up.
        size_t n = indices.size();
        if (n != getter.size()) {
            throw std::runtime_error("fewer instances of type \"external\" than expected from 'ext'");
        }

        std::sort(indices.begin(), indices.end());
        for (size_t i = 0; i < n; ++i) {
            if (i != indices[i]) {
                throw std::runtime_error("set of \"index\" values for type \"external\" should be consecutive starting from zero");
            }
        }
    }
};

}

#endif
