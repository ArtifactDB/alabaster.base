#ifndef UZUKI_VALIDATE_HPP
#define UZUKI_VALIDATE_HPP

#include "unpack.hpp"
#include "Dummy.hpp"
#include "parse.hpp"

#include <vector>
#include <algorithm>
#include <stdexcept>

/**
 * @file validate.hpp
 *
 * @brief Validate JSON file contents against the **uzuki** spec.
 */

namespace uzuki {

/**
 * Validate JSON file contents against the **uzuki** specification.
 * Any invalid representations will cause an error to be thrown.
 *
 * @tparam Json A [`nlohmann::json`](https://github.com/nlohmann/json)-compatible representation of JSON data.
 *
 * @param contents Parsed contents of the JSON file.
 * @param num_external Expected number of external references to "other" objects.
 */
template<class Json>
void validate(const Json& contents, size_t num_external) {
    DummyExternals others(num_external);
    parse<DummyProvisioner>(contents, std::move(others));
    return;
}

/**
 * Validate JSON file contents against the **uzuki** specification with an unknown number of external references.
 * Any invalid representations will cause an error to be thrown.
 *
 * @tparam Json A [`nlohmann::json`](https://github.com/nlohmann/json)-compatible representation of JSON data.
 *
 * @param contents Parsed contents of the JSON file.
 */
template<class Json>
size_t validate(const Json& contents) {
    DummyExternals others(-1);
    ExternalTracker etrack(std::move(others));
    unpack<DummyProvisioner>(contents, etrack);
    check_external_indices(etrack.indices);
    return etrack.indices.size();
}

}

#endif
