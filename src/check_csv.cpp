#include "Rcpp.h"
#include "comservatory/ReadCsv.hpp"
#include "byteme/GzipFileReader.hpp"

//[[Rcpp::export(rng=false)]]
Rcpp::RObject check_csv(std::string path, bool is_compressed, bool parallel) {
    comservatory::ReadCsv fun;
    fun.parallel = parallel;
    fun.validate_only = true;

    if (is_compressed) {
        byteme::GzipFileReader reader(path);
        fun.read(reader);
    } else {
        byteme::RawFileReader reader(path);
        fun.read(reader);
    }

    return R_NilValue;
}
