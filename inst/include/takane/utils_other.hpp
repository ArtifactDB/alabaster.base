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
/**
 * @endcond
 */

namespace internal_other {

inline bool ends_with(const std::string& full, const std::string& sub) {
    return (full.size() >= sub.size() && full.find(sub) == full.size() - sub.size());
}

inline void validate_mcols(const std::filesystem::path& path, size_t expected, const Options& options) {
    if (!std::filesystem::exists(path)) {
        return;
    }

    auto xtype = read_object_type(path);
    if (!ends_with(xtype, "data_frame")) {
        throw std::runtime_error("expected a 'data_frame' or one of its derivatives");
    }
    ::takane::validate(path, xtype, options);

    if (::takane::height(path, xtype, options) != expected) {
        throw std::runtime_error("unexpected number of rows");
    }
}

inline void validate_metadata(const std::filesystem::path& path, const Options& options) {
    if (!std::filesystem::exists(path)) {
        return;
    }

    auto xtype = read_object_type(path);
    if (!ends_with(xtype, "simple_list")) {
        throw std::runtime_error("expected a 'simple_list' or one of its derivatives");
    }
    ::takane::validate(path, xtype, options);
}

}

}

#endif
