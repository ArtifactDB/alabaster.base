#ifndef UZUKI2_VERSIONED_BASE_HPP
#define UZUKI2_VERSIONED_BASE_HPP

/**
 * @file Version.hpp
 * @brief Version-related definitions.
 */

namespace uzuki2 {

/**
 * @brief Version number.
 */
struct Version {
    /**
     * @cond
     */
    Version() = default;
    Version(int maj, int min) {
        major = maj;
        minor = min;
    }
    /**
     * @endcond
     */

    /**
     * Major version number, must be positive.
     */
    int major = 1;

    /**
     * Minor version number, must be non-negative.
     */
    int minor = 0;

    /**
     * @param maj Major version number.
     * @param min Minor version number.
     * @return Whether the version is equal to `<maj>.<min>`.
     */
    bool eq(int maj, int min) const {
        return (major == maj && minor == min);
    }

    bool equals(int maj, int min) const {
        return eq(maj, min);
    }

    /**
     * @param maj Major version number.
     * @param min Minor version number.
     * @return Whether the version is less than `<maj>.<min>`.
     */
    bool lt(int maj, int min) const {
        if (major == maj) {
            return minor < min;
        }
        return major < maj;
    }
};

}

#endif
