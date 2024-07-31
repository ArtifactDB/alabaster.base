#ifndef TAKANE_DERIVED_FROM_HPP
#define TAKANE_DERIVED_FROM_HPP

#include <unordered_set>
#include <unordered_map>
#include <string>

#include "utils_public.hpp"

/**
 * @file _derived_from.hpp
 * @brief Check for derived object relationships.
 */

namespace takane {

/**
 * @cond
 */
namespace internal_derived_from {

inline void fill(const std::unordered_map<std::string, std::unordered_set<std::string> >& registry, std::unordered_set<std::string>& host, const std::string& derived) {
    auto it = registry.find(derived);
    if (it != registry.end()) {
        for (auto d : it->second) {
            host.insert(d);
            fill(registry, host, d);
        }
    }
}

inline auto default_registry() {
    std::unordered_map<std::string, std::unordered_set<std::string> > registry;
    registry["summarized_experiment"] = { "ranged_summarized_experiment" };
    registry["ranged_summarized_experiment"] = { "single_cell_experiment" };
    registry["single_cell_experiment"] = { "spatial_experiment" };

    // Recursively fill the registry to expand the children.
    for (auto& p : registry) {
        auto& host = p.second;
        std::vector<std::string> copy(host.begin(), host.end());
        for (const auto& d : copy) {
            host.insert(d);
            fill(registry, host, d);
        }
    }

    return registry;
}

inline bool check(const std::string& type, const std::string& base, const std::unordered_map<std::string, std::unordered_set<std::string> >& registry) {
    auto it = registry.find(base);
    if (it != registry.end()) {
        const auto& listing = it->second;
        return (listing.find(type) != listing.end());
    }
    return false;
}

}
/**
 * @endcond
 */

/**
 * Check whether a particular object type is derived from a base object type.
 * Derived types satisfy the same file requirements of the base type, but usually add more files to represent additional functionality.
 * This can be used by specifications to check whether arbitrary objects satisfy the file structure expectations for a particular base type.
 *
 * Applications can add their own derived types for a given base class in `Options::custom_derived_from`.
 * This extends the default relationships whereby `derived_from()` will take the union of all derived object types in the default and custom sets.
 * Note that derived types must be manually included in every base type's set, 
 * e.g., if B is derived from A and C is derived from B, C must be added to the sets for both A and B.
 *
 * @param type Object type.
 * @param base Base object type.
 * @param options Validation options, containing custom derived/base relationships.
 * @returns Whether `type` is derived from `base` or is equal to `base`.
 */
inline bool derived_from(const std::string& type, const std::string& base, const Options& options) {
    if (type == base) { 
        return true;
    }
    static const auto derived_from_registry = internal_derived_from::default_registry();
    return internal_derived_from::check(type, base, derived_from_registry) || internal_derived_from::check(type, base, options.custom_derived_from);
}

}

#endif
