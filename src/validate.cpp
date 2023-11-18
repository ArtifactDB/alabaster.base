#include "Rcpp.h"
#include "takane/string_factor.hpp"
#include "takane/atomic_vector.hpp"
#include "takane/simple_list.hpp"
#include "takane/validate.hpp"

//[[Rcpp::export(rng=false)]]
Rcpp::RObject validate_string_factor(std::string path) {
    takane::string_factor::validate(path);
    return R_NilValue;
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject validate_atomic_vector(std::string path) {
    takane::atomic_vector::validate(path);
    return R_NilValue;
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject validate_simple_list(std::string path) {
    takane::simple_list::validate(path);
    return R_NilValue;
}
