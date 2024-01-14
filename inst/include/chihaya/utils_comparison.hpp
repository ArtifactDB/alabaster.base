#ifndef CHIHAYA_UTILS_COMPARISON_HPP
#define CHIHAYA_UTILS_COMPARISON_HPP

#include <string>

namespace chihaya {

namespace internal_comparison {

inline bool is_valid_operation(const std::string& method) {
    return (
        method == "==" ||
        method == ">" ||
        method == "<" ||
        method == ">=" ||
        method == "<=" ||
        method == "!="
    );
}

}

}

#endif
