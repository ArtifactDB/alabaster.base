#include "Rcpp.h"

#include "uzuki2/uzuki2.hpp"

// [[Rcpp::export(rng=false)]]
SEXP check_list_hdf5(std::string file, std::string name, int num_external) {
    uzuki2::hdf5::validate(file, name, num_external, {});
    return R_NilValue;
}

// [[Rcpp::export(rng=false)]]
SEXP check_list_json(std::string file, int num_external, bool parallel) {
    uzuki2::json::Options opt;
    opt.parallel = parallel;
    uzuki2::json::validate_file(file, num_external, opt);
    return R_NilValue;
}
