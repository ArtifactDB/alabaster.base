#include "Rcpp.h"
#include "takane/takane.hpp"

#include <filesystem>
#include <string>
#include <stdexcept>

std::shared_ptr<millijson::Base> convert_to_millijson(Rcpp::RObject x) {
    std::shared_ptr<millijson::Base> output;

    if (x.isNULL()) {
        output.reset(new millijson::Nothing);

    } else if (x.sexp_type() == LGLSXP) {
        Rcpp::LogicalVector y(x);
        if (y.size() != 1) {
            throw std::runtime_error("logical vectors should be of length 1");
        }
        output.reset(new millijson::Boolean(y[0]));

    } else if (x.sexp_type() == INTSXP) {
        Rcpp::IntegerVector y(x);
        if (y.size() != 1) {
            throw std::runtime_error("integer vectors should be of length 1");
        }
        output.reset(new millijson::Number(y[0]));

    } else if (x.sexp_type() == REALSXP) {
        Rcpp::NumericVector y(x);
        if (y.size() != 1) {
            throw std::runtime_error("numeric vectors should be of length 1");
        }
        output.reset(new millijson::Number(y[0]));

    } else if (x.sexp_type() == STRSXP) {
        Rcpp::StringVector y(x);
        if (y.size() != 1) {
            throw std::runtime_error("string vectors should be of length 1");
        }
        output.reset(new millijson::String(Rcpp::as<std::string>(y[0])));

    } else if (x.sexp_type() == VECSXP) {
        Rcpp::List y(x);

        if (y.hasAttribute("names")) {
            auto optr = new millijson::Object;
            output.reset(optr);
            Rcpp::CharacterVector names = y.names();
            for (size_t e = 0, end = names.size(); e < end; ++e) {
                auto field = Rcpp::as<std::string>(names[e]);
                optr->add(std::move(field), convert_to_millijson(y[e]));
            } 

        } else {
            auto aptr = new millijson::Array;
            output.reset(aptr);
            for (size_t e = 0, end = y.size(); e < end; ++e) {
                aptr->add(convert_to_millijson(y[e]));
            } 
        }

    } else {
        throw std::runtime_error("unknown SEXP type '" + std::to_string(x.sexp_type()) + "'");
    }

    return output;
}

Rcpp::RObject convert_to_R(const millijson::Base* x) {
    if (x->type() == millijson::NOTHING) {
        return R_NilValue;
    } else if (x->type() == millijson::BOOLEAN) {
        return Rcpp::LogicalVector::create(reinterpret_cast<const millijson::Boolean*>(x)->value);
    } else if (x->type() == millijson::NUMBER) {
        return Rcpp::NumericVector::create(reinterpret_cast<const millijson::Number*>(x)->value);
    } else if (x->type() == millijson::STRING) {
        return Rcpp::CharacterVector::create(reinterpret_cast<const millijson::String*>(x)->value);
    } else if (x->type() == millijson::ARRAY) {
        const auto& y = reinterpret_cast<const millijson::Array*>(x)->values;
        Rcpp::List output(y.size());
        for (size_t i = 0, end = y.size(); i < end; ++i) {
            output[i] = convert_to_R(y[i].get());
        }
        return output;
    } else if (x->type() == millijson::OBJECT) {
        const auto& y = reinterpret_cast<const millijson::Object*>(x)->values;
        Rcpp::List output(y.size());
        Rcpp::CharacterVector names(y.size());
        size_t i = 0;
        for (const auto& pair : y) {
            names[i] = pair.first;
            output[i] = convert_to_R(pair.second.get());
            ++i;
        }
        output.names() = names;
        return output;
    } else {
        throw std::runtime_error("unknown millijson type '" + std::to_string(x->type()) + "'");
    }
    return R_NilValue;
}

Rcpp::RObject convert_to_R(const takane::ObjectMetadata& x) {
    Rcpp::List output(x.other.size() + 1);
    Rcpp::CharacterVector names(output.size());
    output[0] = Rcpp::CharacterVector::create(x.type);
    names[0] = "type";
    size_t i = 1;
    for (const auto& pair : x.other) {
        names[i] = pair.first;
        output[i] = convert_to_R(pair.second.get());
        ++i;
    }
    output.names() = names;
    return output;
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject validate(std::string path, Rcpp::RObject metadata) {
    if (metadata.isNULL()) {
        takane::validate(path, takane::Options());
    } else {
        auto converted = convert_to_millijson(metadata);
        auto objmeta = takane::reformat_object_metadata(converted.get());
        takane::validate(path, objmeta, takane::Options());
    }
    return R_NilValue;
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject register_validate_function(std::string type, Rcpp::Function fun) {
    auto it = takane::validate_registry.find(type);
    if (it != takane::validate_registry.end()) {
        throw std::runtime_error("validateObject function has already been registered for object type '" + type + "'");
    }
    takane::validate_registry[type] = [fun](const std::filesystem::path& path, const takane::ObjectMetadata& metadata, const takane::Options&) {
        return fun(Rcpp::String(path.c_str()), convert_to_R(metadata));
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
    takane::height_registry[type] = [fun](const std::filesystem::path& path, const takane::ObjectMetadata& metadata, const takane::Options&) -> size_t {
        Rcpp::IntegerVector output = fun(Rcpp::String(path.c_str()), convert_to_R(metadata));
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
        takane::data_frame_factor::any_duplicated = [](const std::filesystem::path& path, const takane::ObjectMetadata& metadata, const takane::Options&) -> bool {
            auto pkg = Rcpp::Environment::namespace_env("alabaster.base");
            Rcpp::Function f = pkg[".anyDuplicated_fallback"];
            Rcpp::IntegerVector output = f(Rcpp::String(path.c_str()), convert_to_R(metadata));
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
