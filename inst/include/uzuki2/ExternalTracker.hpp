#ifndef UZUKI2_EXTERNAL_TRACKER_HPP
#define UZUKI2_EXTERNAL_TRACKER_HPP

#include <vector>
#include <stdexcept>
#include <algorithm>

namespace uzuki2 {

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
