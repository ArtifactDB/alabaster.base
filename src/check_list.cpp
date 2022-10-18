#include "Rcpp.h"

#include "uzuki2/uzuki2.hpp"

// [[Rcpp::export(rng=false)]]
SEXP check_list_hdf5(std::string file, std::string name, int num_external) {
    uzuki2::validate_hdf5(file, name, num_external);
    return R_NilValue;
}

// [[Rcpp::export(rng=false)]]
SEXP check_list_json(std::string file, int num_external) {
    uzuki2::validate_json(file, num_external);
    return R_NilValue;
}
