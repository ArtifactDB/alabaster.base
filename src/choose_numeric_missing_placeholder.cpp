#include "Rcpp.h"
#include "ritsuko/ritsuko.hpp"

//[[Rcpp::export(rng=false)]]
bool any_actually_numeric_na(Rcpp::NumericVector x) {
    for (auto y : x) {
        if (ISNA(y)) {
            return true;
        }
    }
    return false;
}

//[[Rcpp::export(rng=false)]]
Rcpp::LogicalVector is_actually_numeric_na(Rcpp::NumericVector x) {
    Rcpp::LogicalVector output(x.size());
    auto oIt = output.begin();
    for (auto y : x) {
        *(oIt++) = ISNA(y);
    }
    return output;
}

//[[Rcpp::export(rng=false)]]
double choose_numeric_missing_placeholder(Rcpp::NumericVector x) {
    bool has_na = false;
    bool has_nan = false;
    for (auto y : x) {
        if (ISNA(y)) {
            has_na = true;
        } else if (ISNAN(y)) {
            has_nan = true;
        }
    }

    // i.e., if NaNs are not mixed with NAs, we can use
    // either as the missing value placeholder.
    if (!has_na || !has_nan) {
        return NA_REAL;
    }

    auto p = ritsuko::choose_missing_float_placeholder(x.begin(), x.end(), /* skip_nan = */ true);
    if (!p.first) {
        throw std::runtime_error("failed to find a suitable numeric placeholder");
    }
    return p.second;
}
