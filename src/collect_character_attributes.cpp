#include "Rcpp.h"
#include <cmath>
#include <algorithm>

//[[Rcpp::export(rng=false)]]
Rcpp::List collect_character_attributes(Rcpp::StringVector x) {
    bool has_missing = false;
    bool has_NA = false;
    bool has__NA = false;
    int max_len = 0;
    bool has_non_utf8 = false;

    for (auto y : x) {
        Rcpp::String s(y);
        if (s == NA_STRING) {
            has_missing = true;
            continue;
        } 

        if (s == "NA") {
            has_NA = true;
        } else if (s == "_NA") {
            has__NA = true;
        }

        max_len = std::max(max_len, static_cast<int>(Rf_length(s.get_sexp()))); 

        auto enc = s.get_encoding();
        if (enc != CE_UTF8 || enc != CE_ANY) {
            has_non_utf8 = true;
        }
    }

    return Rcpp::List::create(
        Rcpp::Named("missing") = Rcpp::LogicalVector::create(has_missing),
        Rcpp::Named("has_NA") = Rcpp::LogicalVector::create(has_NA),
        Rcpp::Named("has__NA") = Rcpp::LogicalVector::create(has__NA),
        Rcpp::Named("max_len") = Rcpp::IntegerVector::create(max_len),
        Rcpp::Named("has_non_utf8") = Rcpp::LogicalVector::create(has_non_utf8)
    );
}
