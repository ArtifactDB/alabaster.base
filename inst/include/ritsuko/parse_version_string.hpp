#ifndef RITSUKO_PARSE_VERSION_HPP
#define RITSUKO_PARSE_VERSION_HPP

#include <stdexcept>
#include <string>

/**
 * @file parse_version_string.hpp
 * @brief Parse version strings.
 */

namespace ritsuko {

/**
 * @brief Version number.
 *
 * This is typically generated from `parse_version_string()`.
 */
struct Version {
    /**
     * @cond
     */
    Version() = default;

    Version(int ma, int mi, int pa) {
        // Don't move to initializer list due to GCC's decision
        // to define a major() macro... thanks guys.
        major = ma;
        minor = mi;
        patch = pa;
    }
    /**
     * @endcond
     */

    /**
     * Major version number.
     */
    int major = 0;

    /**
     * Minor version number.
     */
    int minor = 0;

    /**
     * Patch version number.
     */
    int patch = 0;

public:
    /**
     * @param maj Major version number.
     * @param min Minor version number.
     * @param pat Patch number.
     * @return Whether the version is equal to `<maj>.<min>.<pat>`.
     */
    bool eq(int maj, int min, int pat) const {
        return (major == maj && minor == min && patch == pat);
    }

    /**
     * @param maj Major version number.
     * @param min Minor version number.
     * @param pat Patch number.
     * @return Whether the version is not equal to `<maj>.<min>.<pat>`.
     */
    bool ne(int maj, int min, int pat) const {
        return !eq(maj, min, pat);
    }

    /**
     * @param maj Major version number.
     * @param min Minor version number.
     * @param pat Patch number.
     * @return Whether the version is less than or equal to `<maj>.<min>.<pat>`.
     */
    bool le(int maj, int min, int pat) const {
        if (major == maj) {
            if (minor == min) {
                return patch <= pat;
            }
            return minor <= min;
        }
        return major <= maj;
    }

    /**
     * @param maj Major version number.
     * @param min Minor version number.
     * @param pat Patch number.
     * @return Whether the version is less than `<maj>.<min>`.
     */
    bool lt(int maj, int min, int pat) const {
        if (major == maj) {
            if (minor == min) {
                return patch < pat;
            }
            return minor < min;
        }
        return major < maj;
    }

    /**
     * @param maj Major version number.
     * @param min Minor version number.
     * @param pat Patch number.
     * @return Whether the version is greater than or equal to `<maj>.<min>.<pat>`.
     */
    bool ge(int maj, int min, int pat) const {
        return !lt(maj, min, pat);
    }

    /**
     * @param maj Major version number.
     * @param min Minor version number.
     * @param pat Patch number.
     * @return Whether the version is greater than `<maj>.<min>.<pat>`.
     */
    bool gt(int maj, int min, int pat) const {
        return !le(maj, min, pat);
    }

public:
    /**
     * @param rhs A `Version` object.
     * @return Whether this version is equal to `rhs`.
     */
    bool operator==(const Version& rhs) const {
        return eq(rhs.major, rhs.minor, rhs.patch);
    }

    /**
     * @param rhs A `Version` object.
     * @return Whether this version is equal to `rhs`.
     */
    bool operator!=(const Version& rhs) const {
        return ne(rhs.major, rhs.minor, rhs.patch);
    }

    /**
     * @param rhs A `Version` object.
     * @return Whether this version is less than `rhs`.
     */
    bool operator<(const Version& rhs) const {
        return lt(rhs.major, rhs.minor, rhs.patch);
    }

    /**
     * @param rhs A `Version` object.
     * @return Whether this version is less than or equal to `rhs`.
     */
    bool operator<=(const Version& rhs) const {
        return le(rhs.major, rhs.minor, rhs.patch);
    }

    /**
     * @param rhs A `Version` object.
     * @return Whether this version is greater than `rhs`.
     */
    bool operator>(const Version& rhs) const {
        return gt(rhs.major, rhs.minor, rhs.patch);
    }

    /**
     * @param rhs A `Version` object.
     * @return Whether this version is greater than or equal to `rhs`.
     */
    bool operator>=(const Version& rhs) const {
        return ge(rhs.major, rhs.minor, rhs.patch);
    }
};

/**
 * @cond
 */
inline void throw_version_error(const char* version_string, size_t size, const char* reason) {
    std::string message(version_string, version_string + size);
    message = "invalid version string '" + message + "' ";
    message += reason;
    throw std::runtime_error(message.c_str());
}
/**
 * @endcond
 */

/**
 * @param[in] version_string Pointer to a version string.
 * @param size Length of the `version_string`.
 * @param skip_patch Whether to skip the patch number.
 * @return A `Version` object containing the version number.
 * If `skip_patch = true`, the `patch` number is always zero.
 */
inline Version parse_version_string(const char* version_string, size_t size, bool skip_patch = false) {
    int major = 0, minor = 0, patch = 0;
    size_t i = 0, end = size;

    // MAJOR VERSION.
    if (size == 0) {
        throw std::runtime_error("version string is empty");
    }
    if (version_string[i] == '0') {
        ++i;
        if (i < end && version_string[i] != '.') {
            throw_version_error(version_string, size, "has leading zeros in its major version");
        }
    } else {
        while (i < end && version_string[i] != '.') {
            if (!std::isdigit(version_string[i])) {
                throw_version_error(version_string, size, "contains non-digit characters");
            }
            major *= 10;
            major += version_string[i] - '0';
            ++i;
        }
    }

    // MINOR VERSION.
    if (i == end) {
        throw_version_error(version_string, size, "is missing a minor version");
    }
    ++i; // get past the period and check again.
    if (i == end) {
        throw_version_error(version_string, size, "is missing a minor version");
    }

    if (version_string[i] != '0') {
        while (i < end && version_string[i] != '.') {
            if (!std::isdigit(version_string[i])) {
                throw_version_error(version_string, size, "contains non-digit characters");
            }
            minor *= 10;
            minor += version_string[i] - '0';
            ++i;
        }

    } else {
        ++i;
        if (i < end && version_string[i] != '.') {
            throw_version_error(version_string, size, "has leading zeros in its minor version");
        }
    }

    if (skip_patch) {
        if (i != end) {
            throw_version_error(version_string, size, "should not have a patch version");
        }
        return Version(major, minor, 0);
    }
 
    // PATCH VERSION.
    if (i == end) {
        throw_version_error(version_string, size, "is missing a patch version");
    }
    ++i; // get past the period and check again.
    if (i == end) {
        throw_version_error(version_string, size, "is missing a patch version");
    }

    if (version_string[i] == '0' && i + 1 < end) {
        throw_version_error(version_string, size, "has leading zeros in its patch version");
    }
    while (i < end) {
        if (!std::isdigit(version_string[i])) {
            throw_version_error(version_string, size, "contains non-digit characters");
        }
        patch *= 10;
        patch += version_string[i] - '0';
        ++i;
    }

    return Version(major, minor, patch);
}

}

#endif
