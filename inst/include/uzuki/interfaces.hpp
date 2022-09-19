#ifndef UZUKI_INTERFACES_HPP
#define UZUKI_INTERFACES_HPP

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

/**
 * @file interfaces.hpp
 * @brief Defines the interfaces to use in JSON parsing.
 */

namespace uzuki {

/**
 * Data type of an embedded R object.
 *
 * - `INTEGER`: 32-bit signed integer vector.
 * - `NUMBER`: double-precision vector.
 * - `STRING`: vector of strings.
 * - `BOOLEAN`: vector of booleans.
 * - `DATE`: vector of date strings in `YYYY-MM-DD` format.
 * - `FACTOR`: factor containing integer indices to unique levels.
 * - `INTEGER_ARRAY`: multi-dimensional array of 32-bit signed integers.
 * - `NUMBER_ARRAY`: multi-dimensional array of doubles.
 * - `STRING_ARRAY`: multi-dimensional array of strings.
 * - `BOOLEAN_ARRAY`: multi-dimensional array of booleans.
 * - `FACTOR_ARRAY`: multi-dimensional array of factors.
 * - `DATE_ARRAY`: multi-dimensional array of date strings.
 * - `DATA_FRAME`: a data frame containing nested arrays or vectors.
 * - `LIST`: a list containing nested objects.
 * - `NOTHING`: equivalent to R's `NULL`.
 * - `OTHER`: an external reference to an unknown R object.
 *
 */
enum Type {
    INTEGER,
    NUMBER,
    STRING,
    BOOLEAN,
    FACTOR,
    DATE,

    INTEGER_ARRAY,
    NUMBER_ARRAY,
    STRING_ARRAY,
    BOOLEAN_ARRAY,
    FACTOR_ARRAY,
    DATE_ARRAY,

    DATA_FRAME,
    LIST,
    NOTHING,
    OTHER
};

/**
 * @param t Data type of an object.
 * @return Whether the object is an array.
 */
inline bool is_array(Type t) {
    return (t >= INTEGER_ARRAY && t < DATA_FRAME);
}

/**
 * @param t Data type of an object.
 * @return Whether the object is a vector.
 */
inline bool is_vector(Type t) {
    return (t >= 0 && t < INTEGER_ARRAY);
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
 * @brief Helper interface for factors.
 */
struct FactorBase {
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
 * @brief Interface for a 1-dimensional factor.
 */
struct Factor : public Vector, public FactorBase {
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
};

/**
 * @brief Interface for a multi-dimensional array.
 */
struct Array : public Base {
    /**
     * @return Extent of the first dimension.
     * For matrices, this would be equivalent to the number of rows.
     */
    virtual size_t first_dim() const = 0;

    /**
     * Indicate that a dimension of the array is named.
     * Each dimension is assumed to be unnamed unless this method is called.
     *
     * @param d A dimension of the array.
     */
    virtual void use_names(size_t d) = 0;

    /**
     * Set the name for an entry along a particular dimension.
     *
     * @param d A dimension of the array.
     * @param i An index along the dimension `d`.
     * @param n Name of entry `i` along dimension `d`.
     */
    virtual void set_name(size_t d, size_t i, std::string n) = 0;

    /**
     * Mark an element of the array as missing.
     *
     * @param i Index of the element in the multi-dimensional array (see `set()`) to be marked as missing.
     * Indices should be computed by treating the array as a contiguous 1-dimensional vector,
     * where the first dimension is the fastest-changing, followed by the second, etc.
     */
    virtual void set_missing(size_t i) = 0;
};

/**
 * @brief Interface for a multi-dimensional array with atomic types.
 *
 * @tparam T Atomic data type of the array.
 * @tparam tt Data `Type` of the array.
 */
template<typename T, Type tt>
struct TypedArray : public Array {
    Type type() const {
        return tt;
    }

    /**
     * Set an element in the array.
     *
     * @param i Index of the element in the multi-dimensional array.
     * Indices should be computed by treating the array as a contiguous 1-dimensional vector,
     * where the first dimension is the fastest-changing, followed by the second, etc.
     * @param v Value of the array element.
     */
    virtual void set(size_t i, T v) = 0;
};

/**
 * Interface for a multi-dimensional array of integers.
 */
typedef TypedArray<int32_t, INTEGER_ARRAY> IntegerArray; 

/**
 * Interface for a multi-dimensional array of doubles.
 */
typedef TypedArray<double, NUMBER_ARRAY> NumberArray;

/**
 * Interface for a multi-dimensional array of strings.
 */
typedef TypedArray<std::string, STRING_ARRAY> StringArray;

/**
 * Interface for a multi-dimensional array of booleans.
 */
typedef TypedArray<unsigned char, BOOLEAN_ARRAY> BooleanArray;

/**
 * Interface for a multi-dimensional array of dates.
 */
typedef TypedArray<std::string, DATE_ARRAY> DateArray;

/**
 * @brief Interface for multi-dimensional factor arrays.
 */
struct FactorArray : public Array, public FactorBase {
    Type type() const {
        return FACTOR_ARRAY;
    }

    /**
     * @param i Index of the element in the multi-dimensional array.
     * Indices should be computed by treating the array as a contiguous 1-dimensional vector,
     * where the first dimension is the fastest-changing, followed by the second, etc.
     * @param v Value of the array element, as an integer code that references the levels.
     */
    virtual void set(size_t i, size_t v) = 0;
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
struct Other : public Base {
    Type type() const {
        return OTHER;
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
    virtual void set(size_t, std::shared_ptr<Base> v) = 0;

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

/**
 * @brief Representation for data frames.
 */
struct DataFrame : public Base {
    Type type() const {
        return DATA_FRAME;
    }

    /**
     * Set a column of the data frame.
     * This requires an associated name for the column.
     *
     * @param i Index of the data frame column.
     * @param n Name of the column.
     * @param v Value of the column.
     */
    virtual void set(size_t i, std::string n, std::shared_ptr<Base> v) = 0;

    /**
     * Indicate that the rows are named.
     * If this is not set, it is assumed that the rows are unnamed.
     */
    virtual void use_names() = 0;

    /**
     * Set the name of a row of the data frame.
     *
     * @param ir Index of the data frame row.
     * @param nr Name of the row.
     */
    virtual void set_name(size_t ir, std::string nr) = 0;
};

}

#endif
