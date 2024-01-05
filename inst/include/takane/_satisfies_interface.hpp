#ifndef TAKANE_SATISFIES_INTERFACE_HPP
#define TAKANE_SATISFIES_INTERFACE_HPP

#include <unordered_set>
#include <unordered_map>
#include <string>
#include "_derived_from.hpp"

namespace takane {

/**
 * @cond
 */
namespace internal_satisfies_interface {

inline auto default_registry() {
    std::unordered_map<std::string, std::unordered_set<std::string> > registry;
    registry["SIMPLE_LIST"] = { "simple_list" };
    registry["DATA_FRAME"] = { "data_frame" };
    registry["SUMMARIZED_EXPERIMENT"] = { "summarized_experiment", "vcf_experiment" };
    return registry;
}

}
/**
 * @endcond
 */

/**
 * Registry of object types that satisfy a particular object interface.
 * Each key is the interface and each value is the set of all types that satisfy it.
 *
 * Applications can extend the **takane** framework by adding custom types to each set.
 * Note that, if a type is included in a particular set, it is not necessary to add its derived types, as `satisfies_interface()` will automatically call `derived_from()`.
 */
inline std::unordered_map<std::string, std::unordered_set<std::string> > satisfies_interface_registry = internal_satisfies_interface::default_registry();

/**
 * Check whether a particular object type satisfies a particular object interface.
 * This can be used by specifications to check that child components satisfy certain expectations.
 *
 * @param type Object type.
 * @param interface Interface type.
 * @returns Whether `type` satisfies `interface`.
 */
inline bool satisfies_interface(const std::string& type, const std::string& interface) {
    auto it = satisfies_interface_registry.find(interface);
    if (it == satisfies_interface_registry.end()) {
        return false;
    }

    const auto& listing = it->second;
    if (listing.find(type) != listing.end()) {
        return true;
    }

    for (const auto& d : listing) {
        if (derived_from(type, d)) {
            return true;
        }
    }

    return false;
}

}

#endif
