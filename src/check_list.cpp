#include "Rcpp.h"

#include "uzuki2/validate.hpp"

// [[Rcpp::export(rng=false)]]
SEXP check_list(std::string file, std::string name, int num_external) {
    uzuki2::validate(file, name, num_external);
    return R_NilValue;
}
