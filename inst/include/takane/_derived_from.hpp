#ifndef TAKANE_DERIVED_FROM_HPP
#define TAKANE_DERIVED_FROM_HPP

#include <unordered_set>
#include <unordered_map>
#include <string>

/**
 * @file _derived_from.hpp
 * @brief Registry of derived object types.
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

}
/**
 * @endcond
 */

/**
 * Registry of derived object types and their base types.
 * Each key is the base object type and each value is the set of all of its derived types.
 * Derived types satisfy the same file requirements of the base type, but usually add more files to represent additional functionality.
 *
 * Applications can extend the **takane** framework by adding custom derived types to each set.
 * Note that derived types must be manually included in every base type's set, 
 * e.g., if B is derived from A and C is derived from B, C must be added to the sets for both A and B.
 */
inline std::unordered_map<std::string, std::unordered_set<std::string> > derived_from_registry = internal_derived_from::default_registry();

/**
 * Check whether a particular object type is derived from a base objct type.
 * This can be used by specifications to check that child components satisfy certain expectations.
 *
 * @param type Object type.
 * @param base Base object type.
 * @returns Whether `type` is derived from `base` or is equal to `base`.
 */
inline bool derived_from(const std::string& type, const std::string& base) {
    if (type == base) { 
        return true;
    }

    auto it = derived_from_registry.find(base);
    if (it == derived_from_registry.end()) {
        return false;
    }

    const auto& listing = it->second;
    return (listing.find(type) != listing.end());
}

}

#endif

