#include "Rcpp.h"

#include "uzuki/parse.hpp"
#include "nlohmann/json.hpp"

/** Defining the simple vectors first. **/

struct RBase {
    virtual Rcpp::RObject extract_object() = 0;
};

template<typename T, uzuki::Type tt, class RVector>
struct RTypedVector : public uzuki::TypedVector<T, tt>, public RBase {
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

typedef RTypedVector<int32_t, uzuki::INTEGER, Rcpp::IntegerVector> RIntegerVector; 

template<>
void RIntegerVector::set_missing(size_t i) {
    vec[i] = NA_INTEGER;
    return;
}

typedef RTypedVector<double, uzuki::NUMBER, Rcpp::NumericVector> RNumberVector;

template<>
void RNumberVector::set_missing(size_t i) {
    vec[i] = NA_REAL;
    return;
}

typedef RTypedVector<std::string, uzuki::STRING, Rcpp::CharacterVector> RStringVector;

template<>
void RStringVector::set_missing(size_t i) {
    vec[i] = NA_STRING;
    return;
}

typedef RTypedVector<unsigned char, uzuki::BOOLEAN, Rcpp::LogicalVector> RBooleanVector;

template<>
void RBooleanVector::set_missing(size_t i) {
    vec[i] = NA_LOGICAL;
    return;
}

typedef RTypedVector<std::string, uzuki::DATE, Rcpp::DateVector> RDateVector;

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

inline void decorate_factor(Rcpp::IntegerVector& vec, const Rcpp::CharacterVector& levels, bool is_ordered) {
    for (auto& i : vec) {
        ++i;
    }
    if (is_ordered) {
        vec.attr("class") = Rcpp::CharacterVector::create("ordered", "factor");
    } else {
        vec.attr("class") = "factor";
    }
    vec.attr("levels") = levels;
    return;
}

struct RFactor : public uzuki::Factor, public RBase {
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
        decorate_factor(vec, levels, ordered);
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

/** Defining arrays. **/

template<class RVector>
void initialize_array(const std::vector<size_t>& dimensions, RVector& data) {
    size_t prod = 1;
    for (auto d_ : dimensions) { prod *= d_; }
    data = RVector(prod);
}

template<class Array>
Rcpp::RObject create_array(Array& obj) {
    auto& vec = obj.vec;
    const auto& dimensions = obj.dimensions;
    vec.attr("dim") = Rcpp::IntegerVector(dimensions.begin(), dimensions.end());

    const auto& names = obj.names;
    Rcpp::List names_(names.size());
    bool any_named = false;
    for (size_t d = 0; d < names.size(); ++d) {
        if (obj.named[d]) {
            any_named = true;
            names_[d] = names[d];
        }
    }

    if (any_named) {
        vec.attr("dimnames") = names_;
    }
    return vec; 
}

template<class Array>
void set_use_names(Array& obj, size_t d) {
    obj.named[d] = true;
    obj.names[d] = Rcpp::CharacterVector(obj.dimensions[d]);
    return;
}

template<typename T, uzuki::Type tt, class RVector>
struct RTypedArray : public uzuki::TypedArray<T, tt>, RBase {
    RTypedArray(std::vector<size_t> d) : vec(0), dimensions(std::move(d)), named(dimensions.size()), names(dimensions.size()) {
        initialize_array(dimensions, vec);
        return;
    }

    size_t first_dim() const {
        return dimensions[0]; 
    }

    void set(size_t i, T val) {
        vec[i] = val;
        return;
    }
    
    void set_missing(size_t);
   
    void use_names(size_t d) {
        set_use_names(*this, d);
        return;
    }

    void set_name(size_t d, size_t i, std::string n) {
        names[d][i] = n;
        return;
    }

    Rcpp::RObject extract_object() { 
        return create_array(*this);
    }

    RVector vec;
    std::vector<size_t> dimensions;
    std::vector<char> named;
    std::vector<Rcpp::CharacterVector> names;
};

typedef RTypedArray<int32_t, uzuki::INTEGER_ARRAY, Rcpp::IntegerVector> RIntegerArray; 

template<>
void RIntegerArray::set_missing(size_t i) {
    vec[i] = NA_INTEGER;
    return;
}

typedef RTypedArray<double, uzuki::NUMBER_ARRAY, Rcpp::NumericVector> RNumberArray;

template<>
void RNumberArray::set_missing(size_t i) {
    vec[i] = NA_REAL;
    return;
}

typedef RTypedArray<std::string, uzuki::STRING_ARRAY, Rcpp::CharacterVector> RStringArray;

template<>
void RStringArray::set_missing(size_t i) {
    vec[i] = NA_STRING;
    return;
}

typedef RTypedArray<unsigned char, uzuki::BOOLEAN_ARRAY, Rcpp::LogicalVector> RBooleanArray;

template<>
void RBooleanArray::set_missing(size_t i) {
    vec[i] = NA_LOGICAL;
    return;
}

typedef RTypedArray<std::string, uzuki::DATE_ARRAY, Rcpp::DateVector> RDateArray;

template<>
void RDateArray::set(size_t i, std::string val) {
    vec[i] = Rcpp::Date(val);
    return;
}

template<>
void RDateArray::set_missing(size_t i) {
    vec[i] = Rcpp::Date(NA_STRING);
    return;
}

struct RFactorArray : public uzuki::FactorArray {
    RFactorArray(std::vector<size_t> d, size_t l) : dimensions(std::move(d)), named(dimensions.size()), names(dimensions.size()), levels(l) {
        initialize_array(dimensions, vec); 
        return;
    }

    size_t first_dim() const {
        return dimensions[0]; 
    }

    void set(size_t i, size_t l) {
        vec[i] = l;
        return;
    }

    void set_missing(size_t i) {
        vec[i] = NA_INTEGER;
        return;
    }

    void use_names(size_t d) {
        set_use_names(*this, d);
        return;
    }

    void set_name(size_t d, size_t i, std::string n) {
        names[d][i] = n;
        return;
    }

    void is_ordered() {
        ordered = true;
        return;
    }

    void set_level(size_t l, std::string lev) {
        levels[l] = lev;
        return;
    }

    Rcpp::RObject extract_object() { 
        decorate_factor(vec, levels, ordered);
        return create_array(*this);
    }

    Rcpp::IntegerVector vec;

    std::vector<size_t> dimensions;
    std::vector<unsigned char> named;
    std::vector<Rcpp::CharacterVector> names;

    bool ordered = false;
    Rcpp::CharacterVector levels;
};

/** Defining the structural elements. **/

struct RNothing : public uzuki::Nothing, public RBase {
    RNothing() {}

    Rcpp::RObject extract_object() { 
        return R_NilValue;
    }
};

struct ROther : public uzuki::Other, public RBase {
    ROther(void* p) : ptr(p) {}

    Rcpp::RObject extract_object() { 
        return *static_cast<Rcpp::RObject*>(ptr);
    }

    void* ptr;
};

struct RList : public uzuki::List, public RBase {
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

struct RDataFrame : public uzuki::DataFrame, public RBase {
    RDataFrame(size_t r, size_t c) : nrows(r), elements(c), colnames(c) {}

    size_t nrow() const { 
        return nrows;
    }

    size_t ncol() const { 
        return elements.size();
    }

    void set(size_t i, std::string n, std::shared_ptr<Base> ptr) {
        elements[i] = dynamic_cast<RBase*>(ptr.get())->extract_object();
        colnames[i] = n;
        return;
    }

    void use_names() {
        named = true;
        rownames = Rcpp::CharacterVector(nrows);
        return;
    }

    void set_name(size_t i, std::string n) {
        rownames[i] = n;
        return;
    }

    Rcpp::RObject extract_object() { 
        Rcpp::List output(elements.begin(), elements.end());
        output.names() = colnames;
        output.attr("class") = "data.frame";
        if (named) {
            output.attr("row.names") = rownames;
        } else {
            Rcpp::IntegerVector defaults(nrows);
            for (size_t i = 0; i < nrows; ++i) { defaults[i] = i + 1; }
            output.attr("row.names") = defaults;
        }
        return output; 
    }

    std::vector<Rcpp::RObject> elements;
    bool named = false;
    Rcpp::CharacterVector rownames, colnames;
    size_t nrows;
};

/** R provisioner. **/

struct RProvisioner {
    static uzuki::Nothing* new_Nothing() { return (new RNothing); }

    static uzuki::Other* new_Other(void *p) { return (new ROther(p)); }

    static uzuki::DataFrame* new_DataFrame(size_t r, size_t c) { return (new RDataFrame(r, c)); }

    static uzuki::List* new_List(size_t l) { return (new RList(l)); }

    static uzuki::IntegerVector* new_Integer(size_t l) { return (new RIntegerVector(l)); }

    static uzuki::NumberVector* new_Number(size_t l) { return (new RNumberVector(l)); }

    static uzuki::StringVector* new_String(size_t l) { return (new RStringVector(l)); }

    static uzuki::BooleanVector* new_Boolean(size_t l) { return (new RBooleanVector(l)); }

    static uzuki::DateVector* new_Date(size_t l) { return (new RDateVector(l)); }

    static uzuki::Factor* new_Factor(size_t l, size_t ll) { return (new RFactor(l, ll)); }

    static uzuki::IntegerArray* new_Integer(std::vector<size_t> d) { return (new RIntegerArray(std::move(d))); }

    static uzuki::NumberArray* new_Number(std::vector<size_t> d) { return (new RNumberArray(std::move(d))); }

    static uzuki::BooleanArray* new_Boolean(std::vector<size_t> d) { return (new RBooleanArray(std::move(d))); }

    static uzuki::StringArray* new_String(std::vector<size_t> d) { return (new RStringArray(std::move(d))); }

    static uzuki::DateArray* new_Date(std::vector<size_t> d) { return (new RDateArray(std::move(d))); }

    static uzuki::FactorArray* new_Factor(std::vector<size_t> d, size_t ll) { return (new RFactorArray(std::move(d), ll)); }
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
Rcpp::RObject load_list(std::string contents, Rcpp::List obj) {
    RExternals others(obj);
    nlohmann::ordered_json j = nlohmann::ordered_json::parse(contents);
    auto ptr = uzuki::parse<RProvisioner>(j, std::move(others));
    return dynamic_cast<RBase*>(ptr.get())->extract_object();
}
