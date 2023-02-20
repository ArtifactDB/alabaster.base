#ifndef UZUKI2_INTERFACES_HPP
#define UZUKI2_INTERFACES_HPP

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

/**
 * @file interfaces.hpp
 * @brief Defines the interfaces to use in HDF5 parsing.
 */

namespace uzuki2 {

/**
 * Data type of an embedded R object.
 *
 * - `INTEGER`: 32-bit signed integer vector.
 * - `NUMBER`: double-precision vector.
 * - `STRING`: vector of strings.
 * - `BOOLEAN`: vector of booleans.
 * - `DATE`: vector of date strings in `YYYY-MM-DD` format.
 * - `FACTOR`: factor containing integer indices to unique levels.
 * - `LIST`: a list containing nested objects.
 * - `NOTHING`: equivalent to R's `NULL`.
 * - `EXTERNAL`: an external reference to an unknown R object.
 */
enum Type {
    INTEGER,
    NUMBER,
    STRING,
    BOOLEAN,
    FACTOR,
    DATE,

    LIST,
    NOTHING,
    EXTERNAL
};

/**
 * @param t Data type of an object.
 * @return Whether the object is a vector.
 */
inline bool is_vector(Type t) {
    return (t >= 0 && t < LIST);
}

/**
 * @brief Base interface for all R objects.
 */
struct Base {
    /**
     * @cond
     */
    virtual ~Base() {}
    /**
     * @endcond
     */

    /**
     * @return The data type. 
     */
    virtual Type type() const = 0;
};

/**
 * @brief Interface for vector-like objects.
 */
struct Vector : public Base {
    /**
     * @return Length of the vector.
     */
    virtual size_t size () const = 0;

    /**
     * Indicate that the elements of the vector are named.
     * If not called, it is assumed that the vector is unnamed.
     */
    virtual void use_names() = 0;

    /**
     * Set the name of a vector element.
     * This method should only be called if `use_names()` has previously been called.
     *
     * @param i Index of a vector element.
     * @param n Name for the vector element.
     */
    virtual void set_name(size_t i, std::string n) = 0;

    /**
     * Indicate that a vector element is missing.
     *
     * @param i Index of a vector element to be marked as missing.
     */
    virtual void set_missing(size_t i) = 0;
};

/**
 * @brief Interface for atomic vectors.
 *
 * @tparam T Data type of the vector elements.
 * @tparam tt `Type` of the vector.
 */
template<typename T, Type tt>
struct TypedVector : public Vector {
    Type type() const {
        return tt;
    }

    /**
     * Set the value of a vector element.
     *
     * @param i Index of a vector element.
     * @param v Value of the vector element.
     */
    virtual void set(size_t i, T v) = 0;

    /**
     * Indicate that a length-1 vector should be treated as a scalar.
     */
    virtual void is_scalar() = 0;
};

/**
 * Interface for an integer vector.
 */
typedef TypedVector<int32_t, INTEGER> IntegerVector; 

/**
 * Interface for a double-precision vector.
 */
typedef TypedVector<double, NUMBER> NumberVector;

/**
 * Interface for a string vector.
 */
typedef TypedVector<std::string, STRING> StringVector;

/**
 * Interface for a boolean vector.
 */
typedef TypedVector<unsigned char, BOOLEAN> BooleanVector;

/**
 * Interface for a date-formatted vector.
 */
typedef TypedVector<std::string, DATE> DateVector;

/**
 * @brief Interface for a factor.
 */
struct Factor : public Vector {
    Type type() const {
        return FACTOR;
    }

    /**
     * Set the value of a factor element.
     *
     * @param i Index of a factor element.
     * @param v Value of the factor element, as an integer index that references the levels.
     */
    virtual void set(size_t i, size_t v) = 0;

    /**
     * Set the levels of the factor.
     *
     * @param il Index of the level element.
     * @param vl Value of the level element.
     */
    virtual void set_level(size_t il, std::string vl) = 0;

    /**
     * Indicate that the factor levels are ordered.
     * If not called, it is assumed that the levels are unordered by default.
     */
    virtual void is_ordered() = 0;
};

/**
 * @brief Representation of R's `NULL`.
 */
struct Nothing : public Base {
    Type type() const {
        return NOTHING;
    }
};

/**
 * @brief Interface for unsupported objects.
 *
 * This usually captures links to external sources that can provide more details on the unsupported object.
 */
struct External : public Base {
    Type type() const {
        return EXTERNAL;
    }
};

/**
 * @brief Interface for lists.
 */
struct List : public Base {
    Type type() const {
        return LIST;
    }

    /**
     * @return Length of the list.
     */
    virtual size_t size() const = 0;

    /**
     * Set an element of the list.
     *
     * @param i Index of the list element.
     * @param v Value of the list element.
     */
    virtual void set(size_t i, std::shared_ptr<Base> v) = 0;

    /**
     * Indicate that the elements of the list are named.
     * If not called, it is assumed that the list is unnamed.
     */
    virtual void use_names() = 0;

    /**
     * Set the name of an element of the list.
     *
     * @param i Index of a list element.
     * @param n Name for the list element.
     */
    virtual void set_name(size_t i, std::string n) = 0;
};

}

#endif
