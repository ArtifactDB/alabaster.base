#include "Rcpp.h"
#include "takane/atomic_vector.hpp"

//[[Rcpp::export(rng=false)]]
Rcpp::RObject validate_atomic_vector(std::string path) {
    takane::atomic_vector::validate(path);
    return R_NilValue;
}
