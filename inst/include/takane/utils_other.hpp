#ifndef TAKANE_UTILS_OTHER_HPP
#define TAKANE_UTILS_OTHER_HPP

#include <filesystem>
#include <string>

#include "utils_public.hpp"

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, const ObjectMetadata&, Options&);
size_t height(const std::filesystem::path&, const ObjectMetadata&, Options&);
bool satisfies_interface(const std::string&, const std::string&, const Options&);
/**
 * @endcond
 */

namespace internal_other {

template<class Reader, typename Path_, typename ... Args_>
Reader open_reader(const Path_& path, Args_&& ... args) {
    if constexpr(std::is_same<typename Path_::value_type, char>::value) {
        return Reader(path.c_str(), std::forward<Args_>(args)...);
    } else {
        return Reader(path.string(), std::forward<Args_>(args)...);
    }
}

inline void validate_mcols(const std::filesystem::path& parent, const std::string& name, size_t expected, Options& options) try {
    auto path = parent / name;
    if (!std::filesystem::exists(path)) {
        return;
    }

    auto xmeta = read_object_metadata(path);
    if (!satisfies_interface(xmeta.type, "DATA_FRAME", options)) {
        throw std::runtime_error("expected an object that satisfies the 'DATA_FRAME' interface");
    }
    ::takane::validate(path, xmeta, options);

    if (::takane::height(path, xmeta, options) != expected) {
        throw std::runtime_error("unexpected number of rows");
    }
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate '" + name + "'; " + std::string(e.what()));
}

inline void validate_metadata(const std::filesystem::path& parent, const std::string& name, Options& options) try {
    auto path = parent / name;
    if (!std::filesystem::exists(path)) {
        return;
    }

    auto xmeta = read_object_metadata(path);
    if (!satisfies_interface(xmeta.type, "SIMPLE_LIST", options)) {
        throw std::runtime_error("expected an object that satisfies the 'SIMPLE_LIST' interface'");
    }
    ::takane::validate(path, xmeta, options);
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate '" + name + "'; " + std::string(e.what()));
}

inline size_t count_directory_entries(const std::filesystem::path& path) {
    size_t num_dir_obj = 0;
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        const auto& p = entry.path().filename().string();
        if (p.size() && (p[0] == '.' || p[0] == '_')) {
            continue;
        }
        ++num_dir_obj;
    }
    return num_dir_obj;
}

}

}

#endif
