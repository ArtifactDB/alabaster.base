#ifndef UZUKI2_UTILS_HPP
#define UZUKI2_UTILS_HPP

#include <cctype>
#include <string>
#include <algorithm>

namespace uzuki2 {

inline bool is_date(const std::string& val) {
    if (val.size() != 10) {
        return false;
    } 
    
    for (size_t p = 0; p < val.size(); ++p) {
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
