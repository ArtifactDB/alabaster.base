#ifndef CHIHAYA_UTILS_PUBLIC_HPP
#define CHIHAYA_UTILS_PUBLIC_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"

#include <string>
#include <functional>
#include <vector>
#include <unordered_map>

/**
 * @file utils_public.hpp
 *
 * @brief Various public utilities.
 */

namespace chihaya {

/**
 * Type of the array.
 * Operations involving mixed types will generally result in promotion to the more advanced types,
 * e.g., an `INTEGER` and `FLOAT` addition will result in promotion to `FLOAT`.
 * Note that operations involving the same types are not guaranteed to preserve type,
 * e.g., `INTEGER` division is assumed to produce a `FLOAT`.
 */
enum ArrayType { BOOLEAN = 0, INTEGER = 1, FLOAT = 2, STRING = 3 }; // giving explicit values for comparisons to work.

/**
 * @brief Details about an array.
 *
 * This contains the type and dimensionality of the array.
 * The exact type representation of the array is left to the implementation;
 * we do not make any guarantees about precision, width or signedness.
 */
struct ArrayDetails {
    /**
     * @cond
     */
    ArrayDetails() {}

    ArrayDetails(ArrayType t, std::vector<size_t> d) : type(t), dimensions(std::move(d)) {}
    /**
     * @endcond
     */

    /**
     * Type of the array.
     */
    ArrayType type;

    /** 
     * Dimensions of the array.
     * Values should be non-negative.
     */
    std::vector<size_t> dimensions;
};

/**
 * @cond
 */
struct State;
/**
 * @endcond
 */

/**
 * Class to map operation types to their validation functions.
 * Each validation function should accept the HDF5 group representing the operation, the version of the **chihaya** specification, and a `State` object.
 */
typedef std::unordered_map<std::string, std::function<ArrayDetails(const H5::Group&, const ritsuko::Version&, State&)> > OperationValidateRegistry;

/**
 * Class to map array types to their validation functions.
 * Each validation function should accept the HDF5 group representing the array and the version of the **chihaya** specification.
 */
typedef std::unordered_map<std::string, std::function<ArrayDetails(const H5::Group&, const ritsuko::Version&)> > ArrayValidateRegistry;

/**
 * @brief Validation state.
 *
 * This is used to store data for a single call to `validate()`.
 * It can be used to override validation functions without modifying the global validation registries, e.g., for parallelized applications.
 * The state may also mutate throughout the duration of the call, allowing callers to collect statistics across the recursive invocations of `validate()` on the same `State` object.
 */
struct State {
    /**
     * Custom registry of functions to be used by `validate()` on arrays.
     * If available for an array type, it takes priority over any function found in the global `chihaya::array_validate_registry`.
     */
    ArrayValidateRegistry array_validate_registry;

    /**
     * Custom registry of functions to be used by `validate()` on operations.
     * If available for an operation type, it takes priority over any function found in the global `chihaya::operation_validate_registry`.
     */
    OperationValidateRegistry operation_validate_registry;
};

}

#endif
