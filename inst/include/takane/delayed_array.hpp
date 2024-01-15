#ifndef TAKANE_DELAYED_ARRAY_HPP
#define TAKANE_DELAYED_ARRAY_HPP

#include "ritsuko/hdf5/hdf5.hpp"
#include "ritsuko/ritsuko.hpp"
#include "chihaya/chihaya.hpp"

#include "utils_public.hpp"
#include "utils_other.hpp"

#include <vector>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <cstdint>

/**
 * @file delayed_array.hpp
 * @brief Validation for delayed arrays.
 */

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, const ObjectMetadata&, const Options&);
bool derived_from(const std::string&, const std::string&);
std::vector<size_t> dimensions(const std::filesystem::path&, const ObjectMetadata&, const Options&);
/**
 * @endcond
 */

/**
 * @namespace takane::delayed_array
 * @brief Definitions for delayed arrays.
 */
namespace delayed_array {

/**
 * @param path Path to the directory containing a delayed array.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, mostly related to reading performance.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, const Options& options) {
    auto vstring = internal_json::extract_version_for_type(metadata.other, "delayed_array");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version '" + vstring + "'");
    }

    chihaya::State state;
    uint64_t max = 0;
    state.array_validate_registry["custom takane seed array"] = [&](const H5::Group& handle, const ritsuko::Version& version) -> chihaya::ArrayDetails {
        auto details = chihaya::custom_array::validate(handle, version);

        auto dhandle = ritsuko::hdf5::open_dataset(handle, "index");
        if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 64, false)) {
            throw std::runtime_error("'index' should have a datatype that fits into a 64-bit unsigned integer");
        }

        auto index = ritsuko::hdf5::load_scalar_numeric_dataset<uint64_t>(dhandle);
        auto seed_path = path / "seeds" / std::to_string(index);
        auto seed_meta = read_object_metadata(seed_path);
        ::takane::validate(seed_path, seed_meta, options);

        auto seed_dims = ::takane::dimensions(seed_path, seed_meta, options);
        if (seed_dims.size() != details.dimensions.size()) {
            throw std::runtime_error("dimensionality of 'seeds/" + std::to_string(index) + "' is not consistent with 'dimensions'");
        }

        for (size_t d = 0, ndims = seed_dims.size(); d < ndims; ++d) {
            if (seed_dims[d] != details.dimensions[d]) {
                throw std::runtime_error("dimension extents of 'seeds/" + std::to_string(index) + "' is not consistent with 'dimensions'");
            }
        }

        if (index >= max) {
            max = index + 1;
        }
        return details;
    };

    auto apath = path / "array.h5";
    auto fhandle = ritsuko::hdf5::open_file(apath);
    auto ghandle = ritsuko::hdf5::open_group(fhandle, "delayed_array");
    ritsuko::Version chihaya_version = chihaya::extract_version(ghandle);
    if (chihaya_version.lt(1, 1, 0)) {
        throw std::runtime_error("version of the chihaya specification should be no less than 1.1");
    }

    chihaya::validate(ghandle, chihaya_version, state);

    size_t found = 0;
    auto seed_path = path / "seeds";
    if (std::filesystem::exists(seed_path)) {
        found = internal_other::count_directory_entries(seed_path);
    }
    if (max != found) {
        throw std::runtime_error("number of objects in 'seeds' is not consistent with the number of 'index' references in 'array.h5'");
    }
}

/**
 * @param path Path to the directory containing a delayed array.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, mostly related to reading performance.
 * @return Extent of the first dimension.
 */
inline size_t height(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] const Options& options) {
    auto apath = path / "array.h5";
    auto output = chihaya::validate(apath, "delayed_array");
    return output.dimensions[0];
}

/**
 * @param path Path to the directory containing a delayed array.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options, mostly related to reading performance.
 * @return Dimensions of the array.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] const Options& options) {
    auto apath = path / "array.h5";
    auto output = chihaya::validate(apath, "delayed_array");
    return std::vector<size_t>(output.dimensions.begin(), output.dimensions.end());
}

}

}

#endif
