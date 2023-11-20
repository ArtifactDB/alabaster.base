#include "Rcpp.h"
#include "takane/takane.hpp"

#include <filesystem>
#include <string>
#include <stdexcept>

//[[Rcpp::export(rng=false)]]
Rcpp::RObject validate(std::string path, std::string type) {
    takane::validate(path, type, takane::Options());
    return R_NilValue;
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject register_validate_function(std::string type, Rcpp::Function fun) {
    auto it = takane::validate_registry.find(type);
    if (it != takane::validate_registry.end()) {
        throw std::runtime_error("validateObject function has already been registered for object type '" + type + "'");
    }
    takane::validate_registry[type] = [fun](const std::filesystem::path& path, const takane::Options&) {
        return fun(Rcpp::String(path.c_str()));
    };
    return R_NilValue;
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject deregister_validate_function(std::string type) {
    auto it = takane::validate_registry.find(type);
    if (it != takane::validate_registry.end()) {
        takane::validate_registry.erase(it);
        return Rf_ScalarLogical(true);
    } else {
        return Rf_ScalarLogical(false);
    }
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject register_height_function(std::string type, Rcpp::Function fun) {
    takane::height_registry[type] = [fun](const std::filesystem::path& path, const takane::Options&) -> size_t {
        Rcpp::IntegerVector output = fun(Rcpp::String(path.c_str()));
        if (output.size() != 1) {
            throw std::runtime_error("expected a integer scalar from height function on '" + path.string() + "'");
        }
        return output[0];
    };
    return R_NilValue;
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject deregister_height_function(std::string type) {
    auto it = takane::height_registry.find(type);
    if (it != takane::height_registry.end()) {
        takane::height_registry.erase(it);
        return Rf_ScalarLogical(true);
    } else {
        return Rf_ScalarLogical(false);
    }
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject register_any_duplicated(bool set) {
    if (set) {
        takane::data_frame_factor::any_duplicated = [](const std::filesystem::path& path, const std::string& type, const takane::Options&) -> bool {
            auto pkg = Rcpp::Environment::namespace_env("alabaster.base");
            Rcpp::Function f = pkg[".anyDuplicated_fallback"];
            Rcpp::IntegerVector output = f(Rcpp::String(path.c_str()), Rcpp::Named("type") = Rcpp::String(type.c_str()));
            if (output.size() != 1) {
                throw std::runtime_error("'anyDuplicated' should return an integer scalar");
            }
            return output[0] != 0;
        };
    } else {
        takane::data_frame_factor::any_duplicated = nullptr;
    }
    return R_NilValue;
}
