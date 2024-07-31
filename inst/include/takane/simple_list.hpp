#ifndef TAKANE_SIMPLE_LIST_HPP
#define TAKANE_SIMPLE_LIST_HPP

#include <string>
#include <stdexcept>
#include <filesystem>

#include "uzuki2/uzuki2.hpp"
#include "byteme/byteme.hpp"

#include "utils_public.hpp"
#include "utils_other.hpp"

/**
 * @file simple_list.hpp
 * @brief Validation for simple lists.
 */

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, Options&);
/**
 * @endcond
 */

/**
 * @namespace takane::simple_list
 * @brief Definitions for simple lists.
 */
namespace simple_list {

/**
 * @cond
 */
namespace internal {

inline std::string extract_format(const internal_json::JsonObjectMap& map) {
    auto fIt = map.find("format");
    if (fIt == map.end()) {
        return "hdf5";
    }
    const auto& val = fIt->second;
    if (val->type() != millijson::STRING) {
        throw std::runtime_error("'simple_list.format' in the object metadata should be a JSON string");
    }
    return reinterpret_cast<millijson::String*>(val.get())->value;
}

inline std::pair<bool, size_t> extract_length(const internal_json::JsonObjectMap& map) {
    auto lIt = map.find("length");
    if (lIt == map.end()) {
        return std::pair<bool, size_t>(false, 0);
    }
    const auto& val = lIt->second;
    if (val->type() != millijson::NUMBER) {
        throw std::runtime_error("'simple_list.length' in the object metadata should be a JSON number");
    }
    return std::pair<bool, size_t>(true, reinterpret_cast<millijson::Number*>(val.get())->value);
}

}
/**
 * @endcond
 */

/**
 * @param path Path to the directory containing the simple list.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    const auto& metamap = internal_json::extract_typed_object_from_metadata(metadata.other, "simple_list");

    const std::string& vstring = internal_json::extract_string_from_typed_object(metamap, "version", "simple_list");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    std::string format = internal::extract_format(metamap);

    auto other_dir = path / "other_contents";
    int num_external = 0;
    if (std::filesystem::exists(other_dir)) {
        auto status = std::filesystem::status(other_dir);
        if (status.type() != std::filesystem::file_type::directory) {
            throw std::runtime_error("expected 'other_contents' to be a directory");
        } 

        num_external = internal_other::count_directory_entries(other_dir);
        for (int e = 0; e < num_external; ++e) {
            auto epath = other_dir / std::to_string(e);
            if (!std::filesystem::exists(epath)) {
                throw std::runtime_error("expected an external list object at '" + std::filesystem::relative(epath, path).string() + "'");
            }

            try {
                ::takane::validate(epath, options);
            } catch (std::exception& e) {
                throw std::runtime_error("failed to validate external list object at '" + std::filesystem::relative(epath, path).string() + "'; " + std::string(e.what()));
            }
        }
    }

    size_t len;
    if (format == "json.gz") {
        uzuki2::json::Options opt;
        opt.parallel = options.parallel_reads;
        auto gzreader = internal_other::open_reader<byteme::GzipFileReader>(path / "list_contents.json.gz");
        auto loaded = uzuki2::json::parse<uzuki2::DummyProvisioner>(gzreader, uzuki2::DummyExternals(num_external), std::move(opt));
        len = reinterpret_cast<const uzuki2::List*>(loaded.get())->size();

    } else if (format == "hdf5") {
        auto handle = ritsuko::hdf5::open_file(path / "list_contents.h5");
        auto ghandle = ritsuko::hdf5::open_group(handle, "simple_list");
        auto loaded = uzuki2::hdf5::parse<uzuki2::DummyProvisioner>(ghandle, uzuki2::DummyExternals(num_external));
        len = reinterpret_cast<const uzuki2::List*>(loaded.get())->size();

    } else {
        throw std::runtime_error("unknown format '" + format + "'");
    }

    if (version.ge(1, 1, 0)) {
        auto len_info = internal::extract_length(metamap);
        if (len_info.first) {
            if (len_info.second != len) {
                throw std::runtime_error("'simple_list.length' differs from the length of the list");
            }
        }
    }
}

/**
 * @param path Path to the directory containing the simple list.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return The number of list elements.
 */
inline size_t height(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    const auto& metamap = internal_json::extract_typed_object_from_metadata(metadata.other, "simple_list");

    auto len_info = internal::extract_length(metamap);
    if (len_info.first) {
        return len_info.second;
    }

    std::string format = internal::extract_format(metamap);
    if (format == "hdf5") {
        auto handle = ritsuko::hdf5::open_file(path / "list_contents.h5");
        auto lhandle = handle.openGroup("simple_list");
        auto vhandle = lhandle.openGroup("data");
        return vhandle.getNumObjs();

    } else {
        // Not much choice but to parse the entire list here. We do so using the
        // dummy, which still has enough self-awareness to hold its own length.
        auto other_dir = path / "other_contents";
        int num_external = 0;
        if (std::filesystem::exists(other_dir)) {
            num_external = internal_other::count_directory_entries(other_dir);
        }

        uzuki2::json::Options opt;
        opt.parallel = options.parallel_reads;
        auto gzreader = internal_other::open_reader<byteme::GzipFileReader>(path / "list_contents.json.gz");
        auto ptr = uzuki2::json::parse<uzuki2::DummyProvisioner>(gzreader, uzuki2::DummyExternals(num_external), std::move(opt));
        return reinterpret_cast<const uzuki2::List*>(ptr.get())->size();
    }
}

}

}

#endif
