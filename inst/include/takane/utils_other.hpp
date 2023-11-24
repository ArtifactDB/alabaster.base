#ifndef TAKANE_UTILS_OTHER_HPP
#define TAKANE_UTILS_OTHER_HPP

#include <filesystem>
#include <string>

#include "utils_public.hpp"

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, const std::string&, const Options&);
size_t height(const std::filesystem::path&, const std::string&, const Options&);
bool satisfies_interface(const std::string&, const std::string&);
/**
 * @endcond
 */

namespace internal_other {

inline void validate_mcols(const std::filesystem::path& parent, const std::string& name, size_t expected, const Options& options) try {
    auto path = parent / name;
    if (!std::filesystem::exists(path)) {
        return;
    }

    auto xtype = read_object_type(path);
    if (!satisfies_interface(xtype, "DATA_FRAME")) {
        throw std::runtime_error("expected an object that satisfies the 'DATA_FRAME' interface");
    }
    ::takane::validate(path, xtype, options);

    if (::takane::height(path, xtype, options) != expected) {
        throw std::runtime_error("unexpected number of rows");
    }
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate '" + name + "'; " + std::string(e.what()));
}

inline void validate_metadata(const std::filesystem::path& parent, const std::string& name, const Options& options) try {
    auto path = parent / name;
    if (!std::filesystem::exists(path)) {
        return;
    }

    auto xtype = read_object_type(path);
    if (!satisfies_interface(xtype, "SIMPLE_LIST")) {
        throw std::runtime_error("expected an object that satisfies the 'SIMPLE_LIST' interface'");
    }
    ::takane::validate(path, xtype, options);
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate '" + name + "'; " + std::string(e.what()));
}

}

}

#endif
