#include "Rcpp.h"
#include "takane/factor.hpp"
#include "byteme/GzipFileReader.hpp"

//[[Rcpp::export(rng=false)]]
Rcpp::RObject check_factor(
    std::string path, 
    int length,
    int num_levels,
    bool has_names,
    bool is_compressed, 
    bool parallel) 
{
    takane::factor::Parameters params;
    params.length = length;
    params.num_levels = num_levels;
    params.has_names = has_names;
    params.parallel = parallel;

    if (is_compressed) {
        byteme::GzipFileReader reader(path);
        takane::factor::validate(reader, params);
    } else {
        byteme::RawFileReader reader(path);
        takane::factor::validate(reader, params);
    }

    return R_NilValue;
}
