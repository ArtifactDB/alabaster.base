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
     * Set the name of a vector element.
     * This method should only be called if the `Vector` instance was constructed with support for names.
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
 * @brief Interface for integer vectors.
 */
struct IntegerVector : public Vector {
    Type type() const {
        return INTEGER;
    }

    /**
     * Set a vector element.
     *
     * @param i Index of a vector element.
     * @param v Value of the vector element.
     */
    virtual void set(size_t i, int32_t v) = 0;
};

/**
 * @brief Interface for a double-precision vector.
 */
struct NumberVector : public Vector {
    Type type() const {
        return NUMBER;
    }

    /**
     * Set a vector element.
     *
     * @param i Index of a vector element.
     * @param v Value of the vector element.
     */
    virtual void set(size_t i, double v) = 0;
};

/**
 * @brief Interface for a string vector.
 */
struct StringVector : public Vector {
    Type type() const {
        return STRING;
    }

    /**
     * Set a vector element.
     *
     * @param i Index of a vector element.
     * @param v Value of the vector element.
     */
    virtual void set(size_t i, std::string v) = 0;

    /**
     * Format constraints to apply to the strings.
     *
     * - `NONE`: no constraints.
     * - `DATE`: strings should be in `YYYY-MM-DD` format.
     * - `DATETIME`: strings should be in Internet Date/Time format (see RFC 3339, section 5.6).
     */
    enum Format {
        NONE,
        DATE,
        DATETIME
    };
};

/**
 * @brief Interface for a boolean vector.
 */
struct BooleanVector : public Vector {
    Type type() const {
        return BOOLEAN;
    }

    /**
     * Set a vector element.
     *
     * @param i Index of a vector element.
     * @param v Value of the vector element.
     */
    virtual void set(size_t i, bool v) = 0;
};

/**
 * @brief Interface for a factor.
 *
 * This is considered a "vector" in terms of its indices, not its levels.
 * So, settings like `Vector::use_names()` and `Vector::is_scalar()` refer to the underlying integer indices.
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
     * Set the name of an element of the list.
     * This should only be called if this `List` instance was constructed with support for names.
     *
     * @param i Index of a list element.
     * @param n Name for the list element.
     */
    virtual void set_name(size_t i, std::string n) = 0;
};

}

#endif
