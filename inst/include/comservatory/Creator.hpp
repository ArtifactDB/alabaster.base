#ifndef COMSERVATORY_CREATOR_HPP
#define COMSERVATORY_CREATOR_HPP

#include <stdexcept>

#include "Field.hpp"

/**
 * @file Creator.hpp
 *
 * @brief Defines the `FieldCreator` class and defaults.
 */

namespace comservatory {

/**
 * @brief Virtual base class for a `Field` creator.
 *
 * Applications may define concrete subclasses to control how `Field`s are created during parsing of the CSV.
 * This involves specializing the `create()` method, possibly with custom `Field` subclasses backed by a different memory mechanism.
 * In this manner, developers can directly stream the CSV into their memory layout rather than going through a `FilledField` intermediate.
 */
struct FieldCreator {
    /**
     * @param t Type of the field.
     * @param n Number of existing (missing) records in the field.
     * @param dummy Whether to create a dummy field. 
     *
     * @return An appropriate instance of a `Field` subclass.
     */
    virtual Field* create(Type t, size_t n, bool dummy) const = 0;

    /**
     * @cond
     */
    virtual ~FieldCreator() {}
    /**
     * @endcond
     */
};

/**
 * @cond
 */
template<bool validate_only>
struct DefaultFieldCreator : public FieldCreator {
    Field* create(Type observed, size_t n, bool dummy) const {
        Field* ptr;

        switch (observed) {
            case STRING:
                if (dummy || validate_only) {
                    ptr = new DummyStringField(n);
                } else {
                    ptr = new FilledStringField(n);
                }
                break;
            case NUMBER:
                if (dummy || validate_only) {
                    ptr = new DummyNumberField(n);
                } else {
                    ptr = new FilledNumberField(n);
                }
                break;
            case BOOLEAN:
                if (dummy || validate_only) {
                    ptr = new DummyBooleanField(n);
                } else {
                    ptr = new FilledBooleanField(n);
                }
                break;
            case COMPLEX:
                if (dummy || validate_only) {
                    ptr = new DummyComplexField(n);
                } else {
                    ptr = new FilledComplexField(n);
                }
                break;
            default:
                throw std::runtime_error("unrecognized type during field creation");
        }

        return ptr;
    }
};
/**
 * @endcond
 */

}

#endif

