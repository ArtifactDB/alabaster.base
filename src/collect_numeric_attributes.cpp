#include "Rcpp.h"
#include <cmath>
#include <cstdint>
#include <algorithm>

//[[Rcpp::export(rng=false)]]
double lowest_double() {
    return std::numeric_limits<double>::lowest();
}

//[[Rcpp::export(rng=false)]]
double highest_double() {
    return std::numeric_limits<double>::max();
}

//[[Rcpp::export(rng=false)]]
Rcpp::List collect_numeric_attributes(Rcpp::NumericVector x) {
    uint8_t has_missing = 0;
    uint8_t has_nan = 0;
    uint8_t has_posinf = 0;
    uint8_t has_neginf = 0;
    uint8_t non_integer = 0;
    uint8_t has_lowest = 0;
    uint8_t has_highest = 0;
    const double lowest = lowest_double();
    const double highest = highest_double();

    for (auto y : x) {
        uint8_t is_na = ISNA(y);
        has_missing |= is_na;
        uint8_t not_na = !is_na;
        has_nan |= (not_na & static_cast<uint8_t>(std::isnan(y)));

        uint8_t is_inf = not_na & static_cast<uint8_t>(std::isinf(y));
        has_posinf |= (is_inf & static_cast<uint8_t>(y > 0));
        has_neginf |= (is_inf & static_cast<uint8_t>(y < 0));

        uint8_t is_finite = not_na & static_cast<uint8_t>(!is_inf);
        non_integer |= (is_finite & static_cast<uint8_t>(std::floor(y) != y));
        has_lowest |= (is_finite & static_cast<uint8_t>(y == lowest));
        has_highest |= (is_finite & static_cast<uint8_t>(y == highest));
    }

    double minv = R_PosInf, maxv = R_NegInf;
    if (!non_integer) {
        if (!has_missing) {
            for (auto y : x) {
                minv = std::min(y, minv);
                maxv = std::max(y, maxv);
            }
        } else {
            for (auto y : x) {
                if (!ISNA(y)) {
                    minv = std::min(y, minv);
                    maxv = std::max(y, maxv);
                }
            }
        }
    }

    return Rcpp::List::create(
        Rcpp::Named("min") = Rcpp::NumericVector::create(minv),
        Rcpp::Named("max") = Rcpp::NumericVector::create(maxv),
        Rcpp::Named("missing") = Rcpp::LogicalVector::create(has_missing),
        Rcpp::Named("non_integer") = Rcpp::LogicalVector::create(non_integer),
        Rcpp::Named("has_NaN") = Rcpp::LogicalVector::create(has_nan),
        Rcpp::Named("has_Inf") = Rcpp::LogicalVector::create(has_posinf),
        Rcpp::Named("has_NegInf") = Rcpp::LogicalVector::create(has_neginf),
        Rcpp::Named("has_lowest") = Rcpp::LogicalVector::create(has_lowest),
        Rcpp::Named("has_highest") = Rcpp::LogicalVector::create(has_highest)
    );
}
