#ifndef TAKANE_UTILS_COMPRESSED_LIST_HPP
#define TAKANE_UTILS_COMPRESSED_LIST_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>
#include <filesystem>

#include "utils_public.hpp"
#include "utils_string.hpp"
#include "utils_other.hpp"
#include "utils_json.hpp"

namespace takane {

void validate(const std::filesystem::path&, const ObjectMetadata&, Options&);
size_t height(const std::filesystem::path&, const ObjectMetadata&, Options&);
bool satisfies_interface(const std::string&, const std::string&, const Options&);
bool derived_from(const std::string&, const std::string&, const Options&);

namespace internal_compressed_list {

inline hsize_t validate_group(const H5::Group& handle, size_t concatenated_length, hsize_t buffer_size) {
    auto lhandle = ritsuko::hdf5::open_dataset(handle, "lengths");
    if (ritsuko::hdf5::exceeds_integer_limit(lhandle, 64, false)) {
        throw std::runtime_error("expected 'lengths' to have a datatype that fits in a 64-bit unsigned integer");
    }

    size_t len = ritsuko::hdf5::get_1d_length(lhandle.getSpace(), false);
    ritsuko::hdf5::Stream1dNumericDataset<uint64_t> stream(&lhandle, len, buffer_size);
    size_t total = 0;
    for (size_t i = 0; i < len; ++i, stream.next()) {
        total += stream.get();
    }
    if (total != concatenated_length) {
        throw std::runtime_error("sum of 'lengths' does not equal the height of the concatenated object (got " + std::to_string(total) + ", expected " + std::to_string(concatenated_length) + ")");
    }

    return len;
}

template<bool satisfies_interface_>
void validate_directory(const std::filesystem::path& path, const std::string& object_type, const std::string& concatenated_type, const ObjectMetadata& metadata, Options& options) try {
    auto vstring = internal_json::extract_version_for_type(metadata.other, object_type);
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    auto catdir = path / "concatenated";
    auto catmeta = read_object_metadata(catdir);
    if constexpr(satisfies_interface_) {
        if (!satisfies_interface(catmeta.type, concatenated_type, options)) {
            throw std::runtime_error("'concatenated' should satisfy the '" + concatenated_type + "' interface");
        }
    } else {
        if (!derived_from(catmeta.type, concatenated_type, options)) {
            throw std::runtime_error("'concatenated' should contain an '" + concatenated_type + "' object");
        }
    }

    try {
        ::takane::validate(catdir, catmeta, options);
    } catch (std::exception& e) {
        throw std::runtime_error("failed to validate the 'concatenated' object; " + std::string(e.what()));
    }
    size_t catheight = ::takane::height(catdir, catmeta, options);

    auto handle = ritsuko::hdf5::open_file(path / "partitions.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, object_type.c_str());
    size_t len = validate_group(ghandle, catheight, options.hdf5_buffer_size);

    internal_string::validate_names(ghandle, "names", len, options.hdf5_buffer_size);
    internal_other::validate_mcols(path, "element_annotations", len, options);
    internal_other::validate_metadata(path, "other_annotations", options);

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate an '" + object_type + "' object at '" + path.string() + "'; " + std::string(e.what()));
}

inline size_t height(const std::filesystem::path& path, const std::string& name, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    auto handle = ritsuko::hdf5::open_file(path / "partitions.h5");
    auto ghandle = handle.openGroup(name);
    auto dhandle = ghandle.openDataSet("lengths");
    return ritsuko::hdf5::get_1d_length(dhandle, false);
}

}

}

#endif
