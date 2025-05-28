#include "Rcpp.h"
#include "comservatory/comservatory.hpp"
#include "byteme/GzipFileReader.hpp"

/** Defining the R fields. **/

template<typename T, comservatory::Type tt, class RVector>
struct RFilledField : public comservatory::TypedField<T, tt> {
    RFilledField(size_t current, size_t max) : position(current), store(max) {
        if (current > max) {
            throw std::runtime_error("more records than specified in preallocation");
        }
        for(size_t i = 0; i < current; ++i) {
            set_NA(store, i);
        }
        return;
    }

    ~RFilledField() {}

    comservatory::Type type() const { return tt; }

    size_t size() const { return position; }

    ::SEXP SEXP() const { return store; }

    void push_back(T in) {
        if (position >= static_cast<size_t>(store.size())) {
            throw std::runtime_error("more records than specified in preallocation");
        }
        store[position] = in;
        ++position;
        return;
    }

    void add_missing() {
        if (position >= static_cast<size_t>(store.size())) {
            throw std::runtime_error("more records than specified in preallocation");
        }
        set_NA(store, position);
        ++position;
        return;
    }

    void set_NA(RVector&, size_t);

    size_t position;
    RVector store;
};

typedef RFilledField<std::string, comservatory::STRING, Rcpp::CharacterVector> RFilledStringField;
typedef RFilledField<double, comservatory::NUMBER, Rcpp::NumericVector> RFilledNumberField;
typedef RFilledField<bool, comservatory::BOOLEAN, Rcpp::LogicalVector> RFilledBooleanField;
typedef RFilledField<std::complex<double>, comservatory::COMPLEX, Rcpp::ComplexVector> RFilledComplexField;

template<>
void RFilledStringField::set_NA(Rcpp::CharacterVector& store, size_t pos) {
    store[pos] = NA_STRING;
    return;
}

template<>
void RFilledNumberField::set_NA(Rcpp::NumericVector& store, size_t pos) {
    store[pos] = NA_REAL;
    return;
}

template<>
void RFilledBooleanField::set_NA(Rcpp::LogicalVector& store, size_t pos) {
    store[pos] = NA_LOGICAL;
    return;
}

template<>
void RFilledComplexField::set_NA(Rcpp::ComplexVector& store, size_t pos) {
    Rcomplex val;
    val.i = NA_REAL;
    val.r = NA_REAL;
    store[pos] = val;
    return;
}

template<>
void RFilledComplexField::push_back(std::complex<double> in) {
    Rcomplex val;
    val.i = in.imag();
    val.r = in.real();
    store[position] = val;
    ++position;
    return;
}

/** Defining the creator. **/

struct RFieldCreator : public comservatory::FieldCreator {
    RFieldCreator(size_t n) : num_records(n) {}

    comservatory::Field* create(comservatory::Type observed, size_t n, bool) const {
        comservatory::Field* ptr;
        
        switch (observed) {
            case comservatory::STRING:
                ptr = new RFilledStringField(n, num_records);
                break;
            case comservatory::NUMBER:
                ptr = new RFilledNumberField(n, num_records);
                break;
            case comservatory::BOOLEAN:
                ptr = new RFilledBooleanField(n, num_records);
                break;
            case comservatory::COMPLEX:
                ptr = new RFilledComplexField(n, num_records);
                break;
            default:
                throw std::runtime_error("unrecognized type during field creation");
        }

        return ptr;
    }

private:
    size_t num_records;
};

/** Defining the actual interface code. **/

// [[Rcpp::export(rng=false)]]
Rcpp::List load_csv(std::string path, bool is_compressed, int nrecords, bool parallel) {
    RFieldCreator creator(nrecords);
    comservatory::ReadOptions opt;
    opt.creator = &creator;
    opt.parallel = parallel;

    comservatory::Contents output;
    if (is_compressed) {
        byteme::GzipFileReader reader(path.c_str(), {});
        output = comservatory::read(reader, opt);
    } else {
        byteme::RawFileReader reader(path.c_str(), {});
        output = comservatory::read(reader, opt);
    }

    Rcpp::List listed(output.num_fields());
    for (size_t o = 0; o < output.num_fields(); ++o) {
        if (output.fields[o]->filled()) {
            switch (output.fields[o]->type()) {
                case comservatory::STRING:
                    listed[o] = static_cast<RFilledStringField*>(output.fields[o].get())->SEXP();
                    break;
                case comservatory::NUMBER:
                    listed[o] = static_cast<RFilledNumberField*>(output.fields[o].get())->SEXP();
                    break;
                case comservatory::BOOLEAN:
                    listed[o] = static_cast<RFilledBooleanField*>(output.fields[o].get())->SEXP();
                    break;
                case comservatory::COMPLEX:
                    listed[o] = static_cast<RFilledComplexField*>(output.fields[o].get())->SEXP();
                    break;
                case comservatory::UNKNOWN:
                    {
                        Rcpp::LogicalVector tmp(output.fields[o]->size());
                        std::fill(tmp.begin(), tmp.end(), NA_LOGICAL);
                        listed[o] = tmp;
                    }
                    break;
                default:
                    throw std::runtime_error("unrecognized type during list assignment");
            }
        }
    }

    listed.names() = Rcpp::StringVector(output.names.begin(), output.names.end());
    if (listed.size() == 0) {
        listed.attr("num.records") = output.num_records();
    }

    return listed;
}

