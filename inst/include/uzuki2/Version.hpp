#ifndef UZUKI2_VERSIONED_BASE_HPP
#define UZUKI2_VERSIONED_BASE_HPP

#include <string>
#include <cstring>

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
    Version(int maj, int min) : major(maj), minor(min) {}
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
    bool equals(int maj, int min) const {
        return (major == maj && minor == min);
    }
};

/**
 * @cond
 */
inline Version parse_version_string(const std::string& version_string) {
    int major = 0, minor = 0;
    size_t i = 0, end = version_string.size();

    if (version_string.empty()) {
        throw std::runtime_error("version string is empty");
    }
    if (version_string[i] == '0') {
        throw std::runtime_error("invalid version string '" + version_string + "' has leading zeros in its major version");
    }
    while (i < end && version_string[i] != '.') {
        if (!std::isdigit(version_string[i])) {
            throw std::runtime_error("invalid version string '" + version_string + "' contains non-digit characters");
        }
        major *= 10;
        major += version_string[i] - '0';
        ++i;
    }

    if (i == end) {
        throw std::runtime_error("version string '" + version_string + "' is missing a minor version");
    }
    ++i; // get past the period and check again.
    if (i == end) {
        throw std::runtime_error("version string '" + version_string + "' is missing a minor version");
    }

    if (version_string[i] == '0' && i + 1 < end) {
        throw std::runtime_error("invalid version string '" + version_string + "' has leading zeros in its minor version");
    }
    while (i < end) {
        if (!std::isdigit(version_string[i])) {
            throw std::runtime_error("invalid version string '" + version_string + "' contains non-digit characters");
        }
        minor *= 10;
        minor += version_string[i] - '0';
        ++i;
    }

    return Version(major, minor);
}
/**
 * @cond
 */

}

#endif
