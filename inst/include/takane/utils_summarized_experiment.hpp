#ifndef TAKANE_UTILS_SUMMARIZED_EXPERIMENT_HPP
#define TAKANE_UTILS_SUMMARIZED_EXPERIMENT_HPP

#include "millijson/millijson.hpp"
#include "utils_json.hpp"

#include <unordered_set>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <cmath>

namespace takane {

namespace internal_summarized_experiment {

inline std::pair<size_t, size_t> extract_dimensions_json(const internal_json::JsonObjectMap& semap, const std::string& type) {
    size_t num_rows = 0, num_cols = 0;

    auto dIt = semap.find("dimensions");
    if (dIt == semap.end()) {
        throw std::runtime_error("expected a '" + type + ".dimensions' property");
    }
    const auto& dims = dIt->second;
    if (dims->type() != millijson::ARRAY) {
        throw std::runtime_error("expected '" + type + ".dimensions' to be an array");
    }

    auto dptr = reinterpret_cast<const millijson::Array*>(dims.get());
    if (dptr->values.size() != 2) {
        throw std::runtime_error("expected '" + type + ".dimensions' to be an array of length 2");
    }

    size_t counter = 0;
    for (const auto& x : dptr->values) {
        if (x->type() != millijson::NUMBER) {
            throw std::runtime_error("expected '" + type + ".dimensions' to be an array of numbers");
        }
        auto val = reinterpret_cast<const millijson::Number*>(x.get())->value;
        if (val < 0 || std::floor(val) != val) {
            throw std::runtime_error("expected '" + type + ".dimensions' to contain non-negative integers");
        }
        if (counter == 0) {
            num_rows = val;
        } else {
            num_cols = val;
        }
        ++counter;
    }

    return std::make_pair(num_rows, num_cols);
}

inline void check_names_json(const std::filesystem::path& dir, std::unordered_set<std::string>& present) try {
    auto parsed = internal_json::parse_file(dir / "names.json");
    if (parsed->type() != millijson::ARRAY) {
        throw std::runtime_error("expected an array");
    }

    auto aptr = reinterpret_cast<const millijson::Array*>(parsed.get());
    size_t number = aptr->values.size();
    present.reserve(number);

    for (size_t i = 0; i < number; ++i) {
        auto eptr = aptr->values[i];
        if (eptr->type() != millijson::STRING) {
            throw std::runtime_error("expected an array of strings");
        }

        auto nptr = reinterpret_cast<const millijson::String*>(eptr.get());
        auto name = nptr->value;
        if (name.empty()) {
            throw std::runtime_error("name should not be an empty string");
        }
        if (present.find(name) != present.end()) {
            throw std::runtime_error("detected duplicated name '" + name + "'");
        }
        present.insert(std::move(name));
    }

} catch (std::exception& e) {
    throw std::runtime_error("invalid '" + dir.string() + "/names.json' file; " + std::string(e.what()));
}

inline size_t check_names_json(const std::filesystem::path& dir) {
    std::unordered_set<std::string> present;
    check_names_json(dir, present);
    return present.size();
}

}

}

#endif
