#ifndef UZUKI2_PARSED_LIST_HPP
#define UZUKI2_PARSED_LIST_HPP

#include <memory>

#include "interfaces.hpp"
#include "Version.hpp"

/**
 * @file ParsedList.hpp
 * @brief Class to hold the parsed list.
 */

namespace uzuki2 {

/**
 * @brief Results of parsing a list from file.
 *
 * This wraps a pointer to `Base` and is equivalent to `shared_ptr<Base>` for most applications.
 * It contains some extra metadata to hold the version information. 
 */
struct ParsedList {
    /**
     * @cond
     */
    ParsedList(std::shared_ptr<Base> p, Version v) : version(std::move(v)), ptr(std::move(p)) {}
    /**
     * @endcond
     */

    /**
     * @return Pointer to `Base`.
     */
    Base* get() const {
        return ptr.get();
    }

    /**
     * @return Reference to `Base`.
     */
    Base& operator*() const {
        return *ptr;
    }

    /**
     * @return Pointer to `Base`.
     */
    Base* operator->() const {
        return ptr.operator->();
    }

    /**
     * @return Whether this stores a non-null pointer.
     */
    operator bool() const {
        return ptr.operator bool();
    }

    /**
     * Calls the corresponding method for `ptr`.
     * @tparam Args_ Assorted arguments.
     * @param args Arguments forwarded to the corresponding method for `ptr`.
     */
    template<typename ...Args_>
    void reset(Args_&& ... args) const {
        ptr.reset(std::forward<Args_>(args)...);
    }

    /**
     * Version of the **uzuki2** specification.
     */
    Version version;

    /**
     * Pointer to the `Base` object.
     */
    std::shared_ptr<Base> ptr;
};

}

#endif
