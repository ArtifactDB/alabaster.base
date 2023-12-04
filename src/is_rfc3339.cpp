#include "ritsuko/ritsuko.hpp"
#include <cstring>
#include "Rcpp.h"

//[[Rcpp::export(rng=false)]]
Rcpp::LogicalVector not_rfc3339(Rcpp::CharacterVector x) {
    size_t n = x.size();
    Rcpp::LogicalVector output(n);
    for (size_t i = 0; i < n; ++i) {
        Rcpp::String current = x[i];
        if (current == NA_STRING) {
            output[i] = false; // technically not in violation as it's missing.
        } else {
            const char* ptr = current.get_cstring();
            size_t len = std::strlen(ptr);
            output[i] = !ritsuko::is_rfc3339(current.get_cstring(), len);
        }
    }
    return output;
}
