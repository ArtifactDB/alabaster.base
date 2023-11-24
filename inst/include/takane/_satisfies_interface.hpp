#ifndef TAKANE_SATISFIES_INTERFACE_HPP
#define TAKANE_SATISFIES_INTERFACE_HPP

#include <unordered_set>
#include <unordered_map>
#include <string>

namespace takane {

/**
 * @cond
 */
namespace internal_satisfies_interface {

inline auto default_registry() {
    std::unordered_map<std::string, std::unordered_set<std::string> > registry;
    registry["SIMPLE_LIST"] = { "simple_list" };
    registry["DATA_FRAME"] = { "data_frame" };
    return registry;
}

}
/**
 * @endcond
 */

/**
 * Registry of object types that satisfy a particular object interface.
 * Each key is the interface and each value is the set of all types that satisfy it.
 * Applications can extend the **takane** framework by adding custom types to each set.
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
    return listing.find(type) != listing.end();
}

}

#endif
