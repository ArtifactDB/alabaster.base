#include "Rcpp.h"
#include "takane/string_factor.hpp"

//[[Rcpp::export(rng=false)]]
Rcpp::RObject validate_string_factor(std::string path) {
    takane::string_factor::validate(path);
    return R_NilValue;
}
