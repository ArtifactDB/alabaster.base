#ifndef RITSUKO_HDF5_LOAD_ATTRIBUTE_HPP
#define RITSUKO_HDF5_LOAD_ATTRIBUTE_HPP

#include "H5Cpp.h"

#include <vector>
#include <string>

#include "get_1d_length.hpp"
#include "as_numeric_datatype.hpp"
#include "_strings.hpp"

/**
 * @file load_attribute.hpp
 * @brief Load HDF5 attributes.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * @param attr Handle to a scalar string attribute.
 * Callers are responsible for checking that `attr` contains a string datatype class.
 * @return The attribute as a string.
 */
inline std::string load_scalar_string_attribute(const H5::Attribute& attr) {
    auto dtype = attr.getDataType();

    // Unfortunately, we can't just do 'std::string output; attr.read(dtype, output);', 
    // as we need to catch NULL pointers in the variable case.

    if (dtype.isVariableStr()) {
        auto mspace = attr.getSpace();
        char* buffer;
        attr.read(dtype, &buffer);
        [[maybe_unused]] VariableStringCleaner deletor(dtype.getId(), mspace.getId(), &buffer);
        if (buffer == NULL) {
            throw std::runtime_error("detected a NULL pointer for a variable length string attribute");
        }
        return std::string(buffer);

    } else {
        size_t len = dtype.getSize();
        std::vector<char> buffer(len);
        attr.read(dtype, buffer.data());
        auto ptr = buffer.data();
        return std::string(ptr, ptr + find_string_length(ptr, len));
    }
}

/**
 * @tparam check_ Whether to check that `attr` is a 1-dimensional string attribute.
 * @param attr Handle to a 1-dimensional string attribute.
 * Callers are responsible for checking that `attr` contains a string datatype class.
 * @param full_length Length of the attribute in `attr`, usually obtained by `get_1d_length()`.
 * @return Vector of strings.
 */
inline std::vector<std::string> load_1d_string_attribute(const H5::Attribute& attr, hsize_t full_length) {
    auto dtype = attr.getDataType();
    auto mspace = attr.getSpace();
    std::vector<std::string> output;
    output.reserve(full_length);

    if (dtype.isVariableStr()) {
        std::vector<char*> buffer(full_length);
        attr.read(dtype, buffer.data());
        [[maybe_unused]] VariableStringCleaner deletor(dtype.getId(), mspace.getId(), buffer.data());
        for (hsize_t i = 0; i < full_length; ++i) {
            if (buffer[i] == NULL) {
                throw std::runtime_error("detected a NULL pointer for a variable length string attribute");
            }
            output.emplace_back(buffer[i]);
        }

    } else {
        size_t len = dtype.getSize();
        std::vector<char> buffer(len * full_length);
        attr.read(dtype, buffer.data());
        auto ptr = buffer.data();
        for (size_t i = 0; i < full_length; ++i, ptr += len) {
            output.emplace_back(ptr, ptr + find_string_length(ptr, len));
        }
    }

    return output;
}

/**
 * Overload of `load_1d_string_attribute()` that determines the length of the attribute via `get_1d_length()`.
 * @param attr Handle to a 1-dimensional string attribute.
 * Callers are responsible for checking that `attr` contains a string datatype class.
 * @return Vector of strings.
 */
inline std::vector<std::string> load_1d_string_attribute(const H5::Attribute& attr) {
    return load_1d_string_attribute(attr, get_1d_length(attr.getSpace(), false));
}

/**
 * @tparam Type_ Type for holding the data in memory, see `as_numeric_datatype()` for supported types.
 * @param attr Handle to a scalar numeric attribute.
 * Callers are responsible for checking that the datatype of `attr` is appropriate for `Type_`, e.g., with `exceeds_integer_limit()`.
 * @return The value of the attribute.
 */
template<typename Type_>
Type_ load_scalar_numeric_attribute(const H5::Attribute& attr) {
    Type_ val;
    const auto& mtype = as_numeric_datatype<Type_>();
    attr.read(mtype, &val);
    return val;
}

/**
 * @tparam Type_ Type for holding the data in memory, see `as_numeric_datatype()` for supported types.
 * @param attr Handle to a numeric attribute.
 * Callers are responsible for checking that the datatype of `attr` is appropriate for `Type_`, e.g., with `exceeds_integer_limit()`.
 * @param full_length Length of the attribute in `attr`, usually obtained by `get_1d_length()`.
 * @return Vector containing the contents of the attribute.
 */
template<typename Type_>
std::vector<Type_> load_1d_numeric_attribute(const H5::Attribute& attr, hsize_t full_length) {
    const auto& mtype = as_numeric_datatype<Type_>();
    std::vector<Type_> buffer(full_length);
    attr.read(mtype, buffer.data());
    return buffer;
}

/**
 * Overload of `load_1d_numeric_attribute()` that determines the length of the attribute via `get_1d_length()`.
 * @tparam Type_ Type for holding the data in memory, see `as_numeric_datatype()` for supported types.
 * @param attr Handle to a numeric attribute.
 * Callers are responsible for checking that the datatype of `attr` is appropriate for `Type_`, e.g., with `exceeds_integer_limit()`.
 * @return Vector containing the contents of the attribute.
 */
template<typename Type_>
std::vector<Type_> load_1d_numeric_attribute(const H5::Attribute& attr) {
    return load_1d_numeric_attribute<Type_>(attr, get_1d_length(attr.getSpace(), false));
}

}

}

#endif
