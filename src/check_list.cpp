#include "Rcpp.h"

#include "uzuki/validate.hpp"
#include "nlohmann/json.hpp"

// [[Rcpp::export(rng=false)]]
int check_list(std::string contents) {
    nlohmann::json j = nlohmann::json::parse(contents);
    return uzuki::validate(j);
}
