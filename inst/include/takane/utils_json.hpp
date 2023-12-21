#ifndef TAKANE_UTILS_JSON_HPP
#define TAKANE_UTILS_JSON_HPP

#include <string>
#include <stdexcept>
#include <unordered_map>
#include <filesystem>

#include "millijson/millijson.hpp"

namespace takane {

namespace internal_json {

typedef std::unordered_map<std::string, std::shared_ptr<millijson::Base> > JsonObjectMap;

template<typename Path_>
std::shared_ptr<millijson::Base> parse_file(const Path_& path) {
    if constexpr(std::is_same<typename Path_::value_type, char>::value) {
        return millijson::parse_file(path.c_str());
    } else {
        auto cpath = path.string();
        return millijson::parse_file(cpath.c_str());
    }
}

inline const JsonObjectMap& extract_object(const JsonObjectMap& x, const std::string& name) {
    auto xIt = x.find(name);
    if (xIt == x.end()) {
        throw std::runtime_error("property is not present");
    }
    const auto& val = xIt->second;
    if (val->type() != millijson::OBJECT) {
        throw std::runtime_error("property should be a JSON object");
    }
    return reinterpret_cast<millijson::Object*>(val.get())->values;
}

inline const std::string& extract_string(const JsonObjectMap& x, const std::string& name) {
    auto xIt = x.find(name);
    if (xIt == x.end()) {
        throw std::runtime_error("property is not present");
    }
    const auto& val = xIt->second;
    if (val->type() != millijson::STRING) {
        throw std::runtime_error("property should be a JSON string");
    }
    return reinterpret_cast<millijson::String*>(val.get())->value;
}

template<class Function_>
const JsonObjectMap& extract_object(const JsonObjectMap& x, const std::string& name, Function_ rethrow) { 
    const JsonObjectMap* output = NULL;
    try {
        output = &(extract_object(x, name));
    } catch (std::exception& e) {
        rethrow(e);
    }
    return *output;
}

template<class Function_>
const std::string& extract_string(const JsonObjectMap& x, const std::string& name, Function_ rethrow) {
    const std::string* output = NULL;
    try {
        output = &(extract_string(x, name));
    } catch (std::exception& e) {
        rethrow(e);
    }
    return *output;
}

inline const JsonObjectMap& extract_typed_object_from_metadata(const JsonObjectMap& x, const std::string& type) {
    return internal_json::extract_object(x, type, [&](std::exception& e) {
        throw std::runtime_error("failed to extract '" + type + "' from the object metadata; " + std::string(e.what())); 
    });
}

inline const std::string& extract_string_from_typed_object(const JsonObjectMap& x, const std::string& name, const std::string& type) {
    return extract_string(x, name, [&](std::exception& e) { 
        throw std::runtime_error("failed to extract '" + type + "." + name + "' from the object metadata; " + std::string(e.what())); 
    });
}

inline const std::string& extract_version_for_type(const JsonObjectMap& x, const std::string& type) {
    const auto& obj = extract_typed_object_from_metadata(x, type);
    return extract_string_from_typed_object(obj, "version", type);
}

}

}

#endif
