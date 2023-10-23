#include "Rcpp.h"
#include "comservatory/comservatory.hpp"
#include "byteme/GzipFileReader.hpp"

//[[Rcpp::export(rng=false)]]
Rcpp::RObject check_csv(std::string path, bool is_compressed, bool parallel) {
    comservatory::ReadOptions opts;
    opts.parallel = parallel;
    opts.validate_only = true;

    if (is_compressed) {
        byteme::GzipFileReader reader(path);
        comservatory::read(reader, opts);
    } else {
        byteme::RawFileReader reader(path);
        comservatory::read(reader, opts);
    }

    return R_NilValue;
}
