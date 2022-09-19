#ifndef COMSERVATORY_TYPE_HPP
#define COMSERVATORY_TYPE_HPP

/**
 * @file Type.hpp
 *
 * @brief Defines the `Type` enumeration for field types.
 */

namespace comservatory {

/**
 * Type of a field in a CSV file.
 */
enum Type {
    STRING,
    NUMBER,
    COMPLEX,
    BOOLEAN,
    UNKNOWN
};

/**
 * @tparam Char Type of character field, typically a `char` or `unsigned char`.
 *
 * @param[in] x Pointer to an array of characters containing a field of interest.
 * @param n Length of the array in `x`.
 */
template<typename Char>
Type decide_type(const Char* x, size_t n) {
    if (!n) {
        throw std::runtime_error("could not determine type for an empty entry");
    }
    
    if (n == 2 && x[0] == 'N' && x[1] == 'A') {
        return UNKNOWN;
    }

    if (x[0] == '"') {
        return STRING;
    }

    if (x[0] == 'T' || x[0] == 't' || x[0] == 'F' || x[0] == 'f') {
        return BOOLEAN;
    }

    if (x[n-1] == 'i') {
        return COMPLEX;
    }

    return NUMBER;
}

/**
 * @param type Type of a field.
 * @return String containing the type of the field.
 */
inline std::string type_to_name (Type type) {
    switch (type) {
        case NUMBER:
            return "NUMBER";
        case STRING:
            return "STRING";
        case BOOLEAN:
            return "BOOLEAN";
        case COMPLEX:
            return "COMPLEX";
        case UNKNOWN:
            return "UNKNOWN";
        default:
            throw std::runtime_error("unrecognized type for name generation");
    }
}

}

#endif
