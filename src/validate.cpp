#include "Rcpp.h"
#include "takane/takane.hpp"

#include <filesystem>
#include <string>
#include <stdexcept>

static takane::Options global_options;

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
        takane::validate(path, global_options);
    } else {
        auto converted = convert_to_millijson(metadata);
        auto objmeta = takane::reformat_object_metadata(converted.get());
        takane::validate(path, objmeta, global_options);
    }
    return R_NilValue;
}

template<class Registry>
bool has_existing(const std::string& type, const Registry& registry, const std::string& existing) {
    auto it = registry.find(type);
    if (it == registry.end()) {
        return false;
    }
    if (existing == "error") {
        throw std::runtime_error("function has already been registered for object type '" + type + "'");
    }
    return (existing == "old");
}

template<class Registry>
Rcpp::RObject deregister(std::string type, Registry& registry) {
    auto it = registry.find(type);
    if (it != registry.end()) {
        registry.erase(it);
        return Rf_ScalarLogical(true);
    } else {
        return Rf_ScalarLogical(false);
    }
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject register_validate_function(std::string type, Rcpp::Function fun, std::string existing) {
    if (!has_existing(type, global_options.custom_validate, existing)) {
        global_options.custom_validate[type] = [fun](const std::filesystem::path& path, const takane::ObjectMetadata& metadata, takane::Options&) {
            fun(Rcpp::String(path.c_str()), convert_to_R(metadata));
            return;
        };
    }
    return R_NilValue;
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject deregister_validate_function(std::string type) {
    return deregister(type, global_options.custom_validate);
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject register_height_function(std::string type, Rcpp::Function fun, std::string existing) {
    if (!has_existing(type, global_options.custom_height, existing)) {
        global_options.custom_height[type] = [fun](const std::filesystem::path& path, const takane::ObjectMetadata& metadata, takane::Options&) -> size_t {
            Rcpp::IntegerVector output = fun(Rcpp::String(path.c_str()), convert_to_R(metadata));
            if (output.size() != 1) {
                throw std::runtime_error("expected a integer scalar from height function on '" + path.string() + "'");
            }
            return output[0];
        };
    }
    return R_NilValue;
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject deregister_height_function(std::string type) {
    return deregister(type, global_options.custom_height);
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject register_dimensions_function(std::string type, Rcpp::Function fun, std::string existing) {
    if (!has_existing(type, global_options.custom_dimensions, existing)) {
        global_options.custom_dimensions[type] = [fun](const std::filesystem::path& path, const takane::ObjectMetadata& metadata, takane::Options&) -> std::vector<size_t> {
            Rcpp::IntegerVector output = fun(Rcpp::String(path.c_str()), convert_to_R(metadata));
            return std::vector<size_t>(output.begin(), output.end());
        };
    }
    return R_NilValue;
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject deregister_dimensions_function(std::string type) {
    return deregister(type, global_options.custom_dimensions);
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject register_any_duplicated(bool set) {
    if (set) {
        global_options.data_frame_factor_any_duplicated = [](const std::filesystem::path& path, const takane::ObjectMetadata& metadata, takane::Options&) -> bool {
            auto pkg = Rcpp::Environment::namespace_env("alabaster.base");
            Rcpp::Function f = pkg[".anyDuplicated_fallback"];
            Rcpp::IntegerVector output = f(Rcpp::String(path.c_str()), convert_to_R(metadata));
            if (output.size() != 1) {
                throw std::runtime_error("'anyDuplicated' should return an integer scalar");
            }
            return output[0] != 0;
        };
    } else {
        global_options.data_frame_factor_any_duplicated = nullptr;
    }
    return R_NilValue;
}
