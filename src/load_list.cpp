#include "Rcpp.h"

#include "uzuki2/uzuki2.hpp"

/** Defining the simple vectors first. **/

struct RBase {
    virtual ~RBase() = default;
    virtual Rcpp::RObject extract_object() = 0;
};

struct RIntegerVector : public uzuki2::IntegerVector, public RBase {
    RIntegerVector(size_t l, bool n, bool) : vec(l), named(n), names(n ? l : 0) {}

    size_t size() const { 
        return vec.size();
    }

    void set(size_t i, int32_t val) {
        vec[i] = val;
        if (val == NA_INTEGER) {
            has_placeholder = true;
        }
    }

    void set_missing(size_t i) {
        vec[i] = NA_INTEGER;
        true_missing.push_back(i);
    }

    void set_name(size_t i, std::string nm) {
        names[i] = nm;
    }

    Rcpp::RObject extract_object() { 
        if (named) {
            vec.names() = names;
        }
        if (!has_placeholder) {
            return vec;
        }

        // If we see that the missing placeholder is present and it's not a
        // true missing value, we promote it to a double-precision vector.
        Rcpp::NumericVector alt(vec.size());
        std::copy(vec.begin(), vec.end(), alt.begin());
        for (auto i : true_missing) {
            alt[i] = R_NaReal;
        }
        if (named) {
            alt.names() = names;
        }
        return alt; 
    }

    Rcpp::IntegerVector vec;
    std::vector<size_t> true_missing;
    bool has_placeholder = false;
    bool named = false;
    Rcpp::CharacterVector names;
};

struct RNumberVector : public uzuki2::NumberVector, public RBase {
    RNumberVector(size_t l, bool n, bool) : vec(l), named(n), names(n ? l : 0) {}

    size_t size() const { 
        return vec.size();
    }

    void set(size_t i, double val) {
        vec[i] = val;
    }

    void set_missing(size_t i) {
        vec[i] = NA_REAL;
    }

    void set_name(size_t i, std::string nm) {
        names[i] = nm;
    }

    Rcpp::RObject extract_object() { 
        if (named) {
            vec.names() = names;
        }
        return vec; 
    }

    Rcpp::NumericVector vec;
    bool named = false;
    Rcpp::CharacterVector names;
};

struct RBooleanVector : public uzuki2::BooleanVector, public RBase {
    RBooleanVector(size_t l, bool n, bool) : vec(l), named(n), names(n ? l : 0) {}

    size_t size() const { 
        return vec.size();
    }

    void set(size_t i, bool val) {
        vec[i] = val;
    }

    void set_missing(size_t i) {
        vec[i] = NA_LOGICAL;
    }

    void set_name(size_t i, std::string nm) {
        names[i] = nm;
    }

    Rcpp::RObject extract_object() { 
        if (named) {
            vec.names() = names;
        }
        return vec; 
    }

    Rcpp::LogicalVector vec;
    bool named = false;
    Rcpp::CharacterVector names;
};

struct RStringVector : public uzuki2::StringVector, public RBase {
    RStringVector(size_t l, bool n, bool) : vec(l), named(n), names(n ? l : 0) {}

    size_t size() const { 
        return vec.size();
    }

    void set(size_t i, std::string val) {
        vec[i] = val;
    }

    void set_missing(size_t i) {
        vec[i] = NA_STRING;
    }

    void set_name(size_t i, std::string nm) {
        names[i] = nm;
    }

    Rcpp::RObject extract_object() { 
        if (named) {
            vec.names() = names;
        }
        return vec; 
    }

    Rcpp::StringVector vec;
    bool named = false;
    Rcpp::CharacterVector names;
};

struct RDateVector : public uzuki2::StringVector, public RBase {
    RDateVector(size_t l, bool n, bool) : vec(l), named(n), names(n ? l : 0) {}

    size_t size() const { 
        return vec.size();
    }

    void set(size_t i, std::string val) {
        vec[i] = Rcpp::Date(val);
    }

    void set_missing(size_t i) {
        vec[i] = Rcpp::Date(NA_STRING);
    }

    void set_name(size_t i, std::string nm) {
        names[i] = nm;
    }

    Rcpp::RObject extract_object() { 
        if (named) {
            vec.names() = names;
        }
        return vec; 
    }

    Rcpp::DateVector vec;
    bool named = false;
    Rcpp::CharacterVector names;
};

struct RDateTimeVector : public uzuki2::StringVector, public RBase {
    RDateTimeVector(size_t l, bool n, bool) : vec(l), named(n), names(n ? l : 0) {}

    size_t size() const { 
        return vec.size();
    }

    void set(size_t i, std::string val) {
        vec[i] = val;
        return;
    }

    void set_missing(size_t i) {
        vec[i] = NA_STRING;
    }

    void set_name(size_t i, std::string nm) {
        names[i] = nm;
        return;
    }

    Rcpp::RObject extract_object() { 
        if (named) {
            vec.names() = names;
        }
        Rcpp::Environment ns = Rcpp::Environment::namespace_env("alabaster.base");
        Rcpp::Function f = ns["as.Rfc3339"];
        return f(vec);
    }

    Rcpp::StringVector vec;
    bool named = false;
    Rcpp::CharacterVector names;
};

/** As do factors. **/

struct RFactor : public uzuki2::Factor, public RBase {
    RFactor(size_t l, bool n, bool, size_t ll, bool o) : vec(l), named(n), names(n ? l : 0), levels(ll), ordered(o) {}

    size_t size() const { return vec.size(); }

    void set(size_t i, size_t l) {
        vec[i] = l;
    }

    void set_missing(size_t i) {
        vec[i] = NA_INTEGER;
    }

    void set_name(size_t i, std::string n) {
        names[i] = n;
    }

    void set_level(size_t l, std::string lev) {
        levels[l] = lev;
    }

    Rcpp::RObject extract_object() {
        for (auto& i : vec) {
            if (i != NA_INTEGER) {
                ++i;
            }
        }

        if (ordered) {
            vec.attr("class") = Rcpp::CharacterVector::create("ordered", "factor");
        } else {
            vec.attr("class") = "factor";
        }
        vec.attr("levels") = levels;

        if (named) {
            vec.names() = names;
        }
        return vec;
    }

private:
    Rcpp::IntegerVector vec;

    bool named = false;
    Rcpp::CharacterVector names;

    Rcpp::CharacterVector levels;
    bool ordered = false;
};

/** Defining the structural elements. **/

struct RNothing : public uzuki2::Nothing, public RBase {
    RNothing() {}

    Rcpp::RObject extract_object() { 
        return R_NilValue;
    }
};

struct RExternal : public uzuki2::External, public RBase {
    RExternal(void* p) : ptr(p) {}

    Rcpp::RObject extract_object() { 
        return *static_cast<Rcpp::RObject*>(ptr);
    }

    void* ptr;
};

struct RList : public uzuki2::List, public RBase {
    RList(size_t l, bool n) : elements(l), named(n), names(n ? l : 0) {}

    size_t size() const { return elements.size(); }

    void set(size_t i, std::shared_ptr<uzuki2::Base> ptr) {
        auto rptr = dynamic_cast<RBase*>(ptr.get());
        elements[i] = rptr->extract_object();
    }

    void set_name(size_t i, std::string n) {
        names[i] = n;
    }

    Rcpp::RObject extract_object() { 
        Rcpp::List output(elements.begin(), elements.end());
        if (named) {
            output.names() = names;
        }
        return output; 
    }

    std::vector<Rcpp::RObject> elements;
    bool named = false;
    Rcpp::CharacterVector names;
};

/** R provisioner. **/

struct RProvisioner {
    static uzuki2::Nothing* new_Nothing() { return (new RNothing); }

    static uzuki2::External* new_External(void *p) { return (new RExternal(p)); }

    template<class ... Args_>
    static uzuki2::List* new_List(Args_&& ... args) { return (new RList(std::forward<Args_>(args)...)); }

    template<class ... Args_>
    static uzuki2::IntegerVector* new_Integer(Args_&& ... args) { return (new RIntegerVector(std::forward<Args_>(args)...)); }

    template<class ... Args_>
    static uzuki2::NumberVector* new_Number(Args_&& ... args) { return (new RNumberVector(std::forward<Args_>(args)...)); }

    static uzuki2::StringVector* new_String(size_t l, bool n, bool s, uzuki2::StringVector::Format f) {
        if (f == uzuki2::StringVector::DATE) {
            return (new RDateVector(l, n, s)); 
        } else if (f == uzuki2::StringVector::DATETIME) {
            return (new RDateTimeVector(l, n, s)); 
        } else {
            return (new RStringVector(l, n, s)); 
        }
    }

    template<class ... Args_>
    static uzuki2::BooleanVector* new_Boolean(Args_&& ... args) { return (new RBooleanVector(std::forward<Args_>(args)...)); }

    template<class ... Args_>
    static uzuki2::Factor* new_Factor(Args_&& ... args) { return (new RFactor(std::forward<Args_>(args)...)); }
};

struct RExternals {
    RExternals(Rcpp::List o) : objects(o.size()) {
        for (size_t i = 0; i < objects.size(); ++i) {
            objects[i] = o[i];
        }
    }

    void* get(size_t i) {
        if (i >= objects.size()) {
            throw std::runtime_error("index request for type \"other\" out of range (" + std::to_string(i) + " out of " + std::to_string(objects.size()) + ")");
        }
        return &objects[i];
    }

    std::vector<Rcpp::RObject> objects;

    size_t size() const {
        return objects.size();
    }
};

// [[Rcpp::export(rng=false)]]
Rcpp::RObject load_list_hdf5(std::string file, std::string name, Rcpp::List obj) {
    RExternals others(obj);
    auto ptr = uzuki2::hdf5::parse<RProvisioner>(file, name, std::move(others));
    return dynamic_cast<RBase*>(ptr.get())->extract_object();
}

// [[Rcpp::export(rng=false)]]
Rcpp::RObject load_list_json(std::string file, Rcpp::List obj, bool parallel) {
    uzuki2::json::Options opt;
    opt.parallel = parallel;
    RExternals others(obj);
    auto ptr = uzuki2::json::parse_file<RProvisioner>(file, std::move(others));
    return dynamic_cast<RBase*>(ptr.get())->extract_object();
}
