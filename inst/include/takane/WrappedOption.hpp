#ifndef TAKANE_WRAPPED_OPTION_HPP
#define TAKANE_WRAPPED_OPTION_HPP

#include <memory>

namespace takane {

/**
 * @brief Wrapped option that supports `const` or ownership.
 *
 * This is a wrapper class that holds both a `const T*` pointer and an instance of `T`.
 * The former allows users to reference a non-owned instance of `T`, enabling memory-efficient passing of large objects to the various validator functions.
 * The lattter allows users to store the instance of `T` directly in the parameter objects, simplifying memory management. 
 * 
 * @tparam T Type of the wrapped object.
 * This should be moveable, copy-constructible and default-constructible.
 */
template<class T>
struct WrappedOption {
    /**
     * Default constructor.
     */
    WrappedOption() = default;

    /**
     * @param ptr Pointer to an existing instance of `T`.
     * The constructed `WrappedOption` object does not own the instance,
     * and it is assumed that the latter outlives the former.
     */
    WrappedOption(const T* ptr) : non_owned(ptr) {}

    /**
     * @param ptr Pointer to an existing instance of `T`.
     * The `WrappedOption` object does not own the instance,
     * and it is assumed that the latter outlives the former.
     */
    WrappedOption& operator=(const T* ptr) {
        non_owned = ptr;
        return *this;
    }

    /**
     * @param val An instance of `T`.
     * The constructed `WrappedOption` object takes ownership of this instance.
     */
    WrappedOption(T val) : owned(new T(std::move(val))) {}

    /**
     * @param val An instance of `T`.
     * The `WrappedOption` object takes ownership of this instance.
     */
    WrappedOption& operator=(T val) {
        non_owned = NULL;
        owned = std::move(val);
        return *this;
    }

public:
    /**
     * @return A reference to `T`.
     */
    const T& operator*() const {
        if (non_owned) {
            return *non_owned;
        } else {
            return owned;
        }
    }

    /**
     * @return A pointer to `T`.
     */
    const T* get() const {
        if (non_owned) {
            return non_owned;
        } else {
            return &owned;
        }
    }

    /**
     * @return A pointer to `T`.
     */
    const T* operator->() const {
        return get();
    }

    /**
     * @return A mutable pointer to `T`.
     */
    T& mutable_ref() {
        if (non_owned) {
            owned = *non_owned;
            non_owned = NULL;
        }
        return owned;
    }

private:
    const T* non_owned = NULL;
    T owned;
};

}

#endif
