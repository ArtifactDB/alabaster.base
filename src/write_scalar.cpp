#include "Rcpp.h"
#include "H5Cpp.h"

// [[Rcpp::export(rng=false)]]
SEXP write_integer_scalar(std::string path, std::string host, std::string name, int val) {
    H5::H5File fhandle(path, H5F_ACC_RDWR);
    auto ghandle = fhandle.openGroup(host);
    auto dhandle = ghandle.createDataSet(name, H5::PredType::NATIVE_INT, H5S_SCALAR);
    dhandle.write(&val, H5::PredType::NATIVE_INT);
    return R_NilValue;
}

// [[Rcpp::export(rng=false)]]
SEXP write_string_scalar(std::string path, std::string host, std::string name, std::string val) {
    H5::H5File fhandle(path, H5F_ACC_RDWR);
    auto ghandle = fhandle.openGroup(host);

    H5::StrType stype(0, val.size());
    auto dhandle = ghandle.createDataSet(name, stype, H5S_SCALAR);
    dhandle.write(val, stype);
    return R_NilValue;
}
