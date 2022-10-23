#include "Rcpp.h"

#include "uzuki2/uzuki2.hpp"

/** Defining the simple vectors first. **/

struct RBase {
    virtual Rcpp::RObject extract_object() = 0;
};

template<typename T, uzuki2::Type tt, class RVector>
struct RTypedVector : public uzuki2::TypedVector<T, tt>, public RBase {
    RTypedVector(size_t s) : vec(s) {}

    size_t size() const { 
        return vec.size();
    }

    void set(size_t i, T val) {
        vec[i] = val;
        return;
    }

    void set_missing(size_t);

    void use_names() {
        named = true;
        names = Rcpp::CharacterVector(vec.size());
        return;
    }

    void set_name(size_t i, std::string nm) {
        names[i] = nm;
        return;
    }

    Rcpp::RObject extract_object() { 
        if (named) {
            vec.names() = names;
        }
        return vec; 
    }

    RVector vec;
    bool named = false;
    Rcpp::CharacterVector names;
};

typedef RTypedVector<int32_t, uzuki2::INTEGER, Rcpp::IntegerVector> RIntegerVector; 

template<>
void RIntegerVector::set_missing(size_t i) {
    vec[i] = NA_INTEGER;
    return;
}

typedef RTypedVector<double, uzuki2::NUMBER, Rcpp::NumericVector> RNumberVector;

template<>
void RNumberVector::set_missing(size_t i) {
    vec[i] = NA_REAL;
    return;
}

typedef RTypedVector<std::string, uzuki2::STRING, Rcpp::CharacterVector> RStringVector;

template<>
void RStringVector::set_missing(size_t i) {
    vec[i] = NA_STRING;
    return;
}

typedef RTypedVector<unsigned char, uzuki2::BOOLEAN, Rcpp::LogicalVector> RBooleanVector;

template<>
void RBooleanVector::set_missing(size_t i) {
    vec[i] = NA_LOGICAL;
    return;
}

typedef RTypedVector<std::string, uzuki2::DATE, Rcpp::DateVector> RDateVector;

template<>
void RDateVector::set(size_t i, std::string val) {
    vec[i] = Rcpp::Date(val);
    return;
}

template<>
void RDateVector::set_missing(size_t i) {
    vec[i] = Rcpp::Date(NA_STRING);
    return;
}

struct RFactor : public uzuki2::Factor, public RBase {
    RFactor(size_t s, size_t l) : vec(s), levels(l) {}

    size_t size() const { return vec.size(); }

    void set(size_t i, size_t l) {
        vec[i] = l;
        return;
    }

    void set_missing(size_t i) {
        vec[i] = NA_INTEGER;
        return;
    }
   
    void use_names() {
        named = true;
        names = Rcpp::CharacterVector(vec.size());
        return;
    }

    void set_name(size_t i, std::string n) {
        names[i] = n;
        return;
    }

    void set_level(size_t l, std::string lev) {
        levels[l] = lev;
        return;
    }

    void is_ordered() {
        ordered = true;
        return;
    }

    Rcpp::RObject extract_object() {
        for (auto& i : vec) {
            ++i;
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

    bool ordered = false;
    Rcpp::CharacterVector levels;
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
    RList(size_t n) : elements(n) {}

    size_t size() const { return elements.size(); }

    void set(size_t i, std::shared_ptr<Base> ptr) {
        auto rptr = dynamic_cast<RBase*>(ptr.get());
        elements[i] = rptr->extract_object();
        return;
    }

    void use_names() {
        named = true;
        names = Rcpp::CharacterVector(elements.size());
        return;
    }

    void set_name(size_t i, std::string n) {
        names[i] = n;
        return;
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

    static uzuki2::List* new_List(size_t l) { return (new RList(l)); }

    static uzuki2::IntegerVector* new_Integer(size_t l) { return (new RIntegerVector(l)); }

    static uzuki2::NumberVector* new_Number(size_t l) { return (new RNumberVector(l)); }

    static uzuki2::StringVector* new_String(size_t l) { return (new RStringVector(l)); }

    static uzuki2::BooleanVector* new_Boolean(size_t l) { return (new RBooleanVector(l)); }

    static uzuki2::DateVector* new_Date(size_t l) { return (new RDateVector(l)); }

    static uzuki2::Factor* new_Factor(size_t l, size_t ll) { return (new RFactor(l, ll)); }
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
    auto ptr = uzuki2::Hdf5Parser().parse<RProvisioner>(file, name, std::move(others));
    return dynamic_cast<RBase*>(ptr.get())->extract_object();
}

// [[Rcpp::export(rng=false)]]
Rcpp::RObject load_list_json(std::string file, Rcpp::List obj, bool parallel) {
    uzuki2::JsonParser parser;
    parser.parallel = parallel;
    RExternals others(obj);
    auto ptr = parser.parse_file<RProvisioner>(file, std::move(others));
    return dynamic_cast<RBase*>(ptr.get())->extract_object();
}
