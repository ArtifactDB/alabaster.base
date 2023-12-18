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
void validate(const std::filesystem::path&, const Options&);
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

template<class Reader, typename ... Args_>
inline Reader open_reader(const std::filesystem::path& path, Args_&& ... args) {
    if constexpr(std::is_same<decltype(path.c_str()), const char*>::value) {
        return Reader(path.c_str(), std::forward<Args_>(args)...);
    } else {
        return Reader(path.string(), std::forward<Args_>(args)...);
    }
}

}
/**
 * @endcond
 */

/**
 * @param path Path to the directory containing the simple list.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, const Options& options) {
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

        for (const auto& entry : std::filesystem::directory_iterator(other_dir)) {
            try {
                ::takane::validate(entry.path(), options);
            } catch (std::exception& e) {
                throw std::runtime_error("failed to validate external list object at '" + std::filesystem::relative(entry.path(), path).string() + "'; " + std::string(e.what()));
            }
            ++num_external;
        }
    }

    if (format == "json.gz") {
        uzuki2::json::Options opt;
        opt.parallel = options.parallel_reads;
        auto gzreader = internal::open_reader<byteme::GzipFileReader>(path / "list_contents.json.gz");
        uzuki2::json::validate(gzreader, num_external, opt);
    } else if (format == "hdf5") {
        auto handle = ritsuko::hdf5::open_file(path / "list_contents.h5");
        auto ghandle = ritsuko::hdf5::open_group(handle, "simple_list");
        uzuki2::hdf5::validate(ghandle, num_external);
    } else {
        throw std::runtime_error("unknown format '" + format + "'");
    }
}

/**
 * @param path Path to the directory containing the simple list.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, typically for reading performance.
 * @return The number of list elements.
 */
inline size_t height(const std::filesystem::path& path, const ObjectMetadata& metadata, const Options& options) {
    const auto& metamap = internal_json::extract_typed_object_from_metadata(metadata.other, "simple_list");
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
        auto gzreader = internal::open_reader<byteme::GzipFileReader>(path / "list_contents.json.gz");
        uzuki2::DummyExternals ext(num_external);
        auto ptr = uzuki2::json::parse<uzuki2::DummyProvisioner>(gzreader, std::move(ext), std::move(opt));
        return reinterpret_cast<const uzuki2::List*>(ptr.get())->size();
    }
}

}

}

#endif
