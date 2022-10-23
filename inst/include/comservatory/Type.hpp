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
