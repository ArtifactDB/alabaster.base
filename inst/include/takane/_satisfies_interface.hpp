#ifndef TAKANE_SATISFIES_INTERFACE_HPP
#define TAKANE_SATISFIES_INTERFACE_HPP

#include <unordered_set>
#include <unordered_map>
#include <string>
#include "_derived_from.hpp"

/**
 * @file _satisfies_interface.hpp
 * @brief Check if an object interface is satisfied.
 */

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

inline bool check(const std::string& type, const std::string& interface, const std::unordered_map<std::string, std::unordered_set<std::string> >& registry, const Options& options) {
    auto it = registry.find(interface);
    if (it == registry.end()) {
        return false;
    }

    const auto& listing = it->second;
    if (listing.find(type) != listing.end()) {
        return true;
    }

    for (const auto& d : listing) {
        if (derived_from(type, d, options)) {
            return true;
        }
    }

    return false;
}

}
/**
 * @endcond
 */

/**
 * Check whether a particular object type satisfies a particular object interface.
 * This can be used by specifications to check that child components satisfy certain user-level expectations for an abstract object (e.g., data frames, lists).
 *
 * Applications can extend the **takane** framework by adding custom types to `Options::custom_satisfies_interface`.
 * This extends the default relationships whereby `satisfies_interface()` will take the union of all object types in the default and custom sets.
 * Note that, if a type is included in a particular set, it is not necessary to add its derived types, as `satisfies_interface()` will automatically call `derived_from()`.
 *
 * @param type Object type.
 * @param interface Interface type.
 * @param options Validation options, containing custom object interface relationships.
 * @returns Whether `type` satisfies `interface`.
 */
inline bool satisfies_interface(const std::string& type, const std::string& interface, const Options& options) {
    static const auto satisfies_interface_registry = internal_satisfies_interface::default_registry();
    return internal_satisfies_interface::check(type, interface, satisfies_interface_registry, options) || internal_satisfies_interface::check(type, interface, options.custom_satisfies_interface, options);
}

}

#endif
