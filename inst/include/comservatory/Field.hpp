#ifndef COMSERVATORY_FIELD_HPP
#define COMSERVATORY_FIELD_HPP

#include <vector>
#include <string>
#include <complex>
#include <numeric>
#include "Type.hpp"

/**
 * @file Field.hpp
 *
 * @brief Defines the `Field` virtual class and concrete implementations.
 */

namespace comservatory {

/**
 * @brief Virtual base class for a describing a field.
 *
 * This can be treated as a vector of values across multiple records in the CSV file. 
 */
struct Field {
    /**
     * @cond
     */
    virtual ~Field() {}
    /**
     * @endcond
     */

    /**
     * @return Current size of the field, i.e., the number of records.
     */
    virtual size_t size() const = 0;

    /**
     * @return Type of the field.
     */
    virtual Type type() const = 0;

    /**
     * Append a missing value onto the field's vector of values.
     */
    virtual void add_missing() = 0;

    /**
     * @return Boolean indicating whether this object contains loaded data.
     * This may be `false` if it is just a dummy for validation/placeholder purposes.
     */
    virtual bool filled() const { 
        return true;
    }
};

/**
 * @brief Field of an unknown type.
 *
 * This is used for all fields at the start of parsing;
 * the type is only resolved upon encountering the first record with a non-missing value.
 * As such, `size()` is implicitly equal to the number of missing records for this field.
 */
struct UnknownField : Field{
    /**
     * @cond
     */
    size_t nrecords = 0;
    /**
     * @endcond
     */

    size_t size() const {
        return nrecords;
    }

    Type type() const {
        return UNKNOWN;
    }

    void add_missing() {
        ++nrecords;
        return;
    }
};

/**
 * @brief Template class for a field of a known type.
 * @tparam T Data type used to store the values.
 * @tparam tt The `Type` to be returned by `type()`.
 */
template<typename T, Type tt>
struct TypedField : public Field {
    Type type() const { return tt; }

    /**
     * @param x Value to be appended to the `TypedField`'s vector of values.
     */
    virtual void push_back(T x) = 0;
};

/**
 * @brief Template class for a typed field that uses a `std::vector` to store its values. 
 * @tparam T Data type used to store the values.
 * @tparam tt The `Type` to be returned by `type()`.
 */
template<typename T, Type tt>
struct FilledField : public TypedField<T, tt> {
    /**
     * @param n Number of missing values with which to fill the field's vector.
     * This may be non-zero if converting a field from an `UnknownField`.
     */
    FilledField(size_t n = 0) : missing(n), values(n) {
        if (n) {
            std::iota(missing.begin(), missing.end(), 0);
        }
    }

    /**
     * @cond
     */
    std::vector<size_t> missing;
    std::vector<T> values;
    /**
     * @endcond
     */

    size_t size() const { 
        return values.size(); 
    }

    void push_back(T x) {
        values.emplace_back(std::move(x));
        return;
    }

    void add_missing() {
        size_t i = values.size();
        missing.push_back(i);
        values.resize(i + 1);
        return;
    }
};

/**
 * @brief Template class for a typed dummy field that only pretends to store its values.
 * @tparam T Data type used to store the values.
 * @tparam tt The `Type` to be returned by `type()`.
 */
template<typename T, Type tt>
struct DummyField : public TypedField<T, tt> {
    /**
     * @param n Number of missing values with which to fill the field's vector.
     * This may be non-zero if converting a field from an `UnknownField`.
     */
    DummyField(size_t n = 0) : nrecords(n) {}

    /**
     * @cond
     */
    size_t nrecords;
    /**
     * @endcond
     */

    size_t size() const {
        return nrecords;
    }

    void push_back(T) {
        ++nrecords;
        return;
    }

    void add_missing() {
        ++nrecords;
        return;
    }

    bool filled() const { 
        return false;
    }
};

/** Various realizations. **/

/**
 * Virtual class for a `Field` of strings.
 */
typedef TypedField<std::string, STRING> StringField;

/**
 * String `Field` with a backing `std::vector<std::string>`.
 */
typedef FilledField<std::string, STRING> FilledStringField;

/**
 * Dummy string `Field`.
 */
typedef DummyField<std::string, STRING> DummyStringField;

/**
 * Virtual class for a `Field` of double-precision numbers.
 */
typedef TypedField<double, NUMBER> NumberField;

/**
 * Numeric `Field` with a backing `std::vector<double>`.
 */
typedef FilledField<double, NUMBER> FilledNumberField;

/**
 * Dummy numeric `Field`.
 */
typedef DummyField<double, NUMBER> DummyNumberField;

/**
 * @brief Virtual class for a `Field` of booleans.
 */
typedef TypedField<char, BOOLEAN> BooleanField;

/**
 * Boolean `Field` with a backing `std::vector<char>`.
 */
typedef FilledField<char, BOOLEAN> FilledBooleanField;

/**
 * Dummy boolean `Field`.
 */
typedef DummyField<char, BOOLEAN> DummyBooleanField;

/**
 * Virtual class for a `Field` of complex numbers.
 */
typedef TypedField<std::complex<double>, COMPLEX> ComplexField;

/**
 * Complex `Field` with a backing `std::vector<std::complex<double> >`.
 */
typedef FilledField<std::complex<double>, COMPLEX> FilledComplexField;

/**
 * Complex `Field`.
 */
typedef DummyField<std::complex<double>, COMPLEX> DummyComplexField;

}

#endif
