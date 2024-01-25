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
void validate(const std::filesystem::path&, const ObjectMetadata&, Options&);
std::vector<size_t> dimensions(const std::filesystem::path&, const ObjectMetadata&, Options&);
/**
 * @endcond
 */

/**
 * @namespace takane::delayed_array
 * @brief Definitions for delayed arrays.
 */
namespace delayed_array {

/**
 * @cond
 */
namespace internal {

// For efficiency purposes, we just mutate the existing
// 'options.delayed_array_options' rather than making a copy. In this case, we
// need to set 'details_only' to either true or false, depending on whether we
// want to do the full validation; it's important to subsequently reset it back
// to its original setting in the destructor.
struct DetailsOnlyResetter {
    DetailsOnlyResetter(chihaya::Options& o) : options(o), old(options.details_only) {}
    ~DetailsOnlyResetter() {
        options.details_only = old;
    }
private:
    chihaya::Options& options;
    bool old;
};

}
/**
 * @endcond
 */

/**
 * @param path Path to the directory containing a delayed array.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    auto vstring = internal_json::extract_version_for_type(metadata.other, "delayed_array");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version '" + vstring + "'");
    }

    uint64_t max = 0;
    {
        std::string custom_name = "custom takane seed array";
        auto& custom_options = options.delayed_array_options;
        bool custom_found = (custom_options.array_validate_registry.find(custom_name) != custom_options.array_validate_registry.end());

        // For efficiency purposes, we just mutate the existing
        // 'options.delayed_array_options' rather than making a copy. We need
        // to add a validator for the 'custom takane seed array' type, which
        // checks for valid references to external arrays in 'seeds/'.
        //
        // Note that we respect any existing 'custom takane seed array' setting
        // - possibly from recursive calls to 'delayed_array::validate()', but
        // also possibly from user-provided overrides, in which case we assume
        // that the caller really knows what they're doing.
        //
        // Anyway, all this means that we only mutate chihaya::Options if there
        // is no existing custom takane function. However, if we do so, we need
        // to restore the original state before function exit, hence the
        // destructor for RAII-based clean-up.
        struct ValidateResetter {
            ValidateResetter(chihaya::Options& o, const std::string& n, bool f) : options(o), name(n), found(f) {}
            ~ValidateResetter() {
                if (!found) {
                    options.array_validate_registry.erase(name);
                }
            }
        private:
            chihaya::Options& options;
            const std::string& name;
            bool found; 
        };
        [[maybe_unused]] ValidateResetter v(custom_options, custom_name, custom_found);

        if (!custom_found) {
            custom_options.array_validate_registry[custom_name] = [&](const H5::Group& handle, const ritsuko::Version& version, chihaya::Options& ch_options) -> chihaya::ArrayDetails {
                auto details = chihaya::custom_array::validate(handle, version, ch_options);

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
        }

        auto apath = path / "array.h5";
        auto fhandle = ritsuko::hdf5::open_file(apath);
        auto ghandle = ritsuko::hdf5::open_group(fhandle, "delayed_array");
        ritsuko::Version chihaya_version = chihaya::extract_version(ghandle);
        if (chihaya_version.lt(1, 1, 0)) {
            throw std::runtime_error("version of the chihaya specification should be no less than 1.1");
        }

        // Again, using RAII to reset the 'details_only' flag to its original
        // state after we're done with it.
        [[maybe_unused]] internal::DetailsOnlyResetter o(custom_options);
        custom_options.details_only = false;

        chihaya::validate(ghandle, chihaya_version, custom_options);
    }

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
 * @param options Validation options.
 * @return Extent of the first dimension.
 */
inline size_t height(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, Options& options) {
    auto& chihaya_options = options.delayed_array_options;
    [[maybe_unused]] internal::DetailsOnlyResetter o(chihaya_options);
    chihaya_options.details_only = true;

    auto apath = path / "array.h5";
    auto fhandle = ritsuko::hdf5::open_file(apath);
    auto ghandle = ritsuko::hdf5::open_group(fhandle, "delayed_array");
    auto output = chihaya::validate(ghandle, chihaya_options);
    return output.dimensions[0];
}

/**
 * @param path Path to the directory containing a delayed array.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return Dimensions of the array.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, Options& options) {
    auto& chihaya_options = options.delayed_array_options;
    [[maybe_unused]] internal::DetailsOnlyResetter o(chihaya_options);
    chihaya_options.details_only = true;

    auto apath = path / "array.h5";
    auto fhandle = ritsuko::hdf5::open_file(apath);
    auto ghandle = ritsuko::hdf5::open_group(fhandle, "delayed_array");
    auto output = chihaya::validate(ghandle, chihaya_options);
    return std::vector<size_t>(output.dimensions.begin(), output.dimensions.end());
}

}

}

#endif
