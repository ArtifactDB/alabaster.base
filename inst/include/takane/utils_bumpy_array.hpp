#ifndef TAKANE_UTILS_BUMPY_ARRAY_HPP
#define TAKANE_UTILS_BUMPY_ARRAY_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>
#include <array>
#include <filesystem>

#include "utils_public.hpp"
#include "utils_array.hpp"
#include "utils_other.hpp"
#include "utils_json.hpp"

namespace takane {

void validate(const std::filesystem::path&, const ObjectMetadata&, Options&);
size_t height(const std::filesystem::path&, const ObjectMetadata&, Options&);
bool satisfies_interface(const std::string&, const std::string&, const Options&);
bool derived_from(const std::string&, const std::string&, const Options&);

namespace internal_bumpy_array {

inline std::vector<uint64_t> validate_dimensions(const H5::Group& handle) {
    auto dhandle = ritsuko::hdf5::open_dataset(handle, "dimensions");
    if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 64, false)) {
        throw std::runtime_error("expected 'dimensions' to have a datatype that fits in a 64-bit unsigned integer");
    }
    auto len = ritsuko::hdf5::get_1d_length(dhandle, false);
    std::vector<uint64_t> output(len);
    dhandle.read(output.data(), H5::PredType::NATIVE_UINT64);
    return output;
}

inline hsize_t validate_lengths(const H5::Group& handle, size_t concatenated_length, hsize_t buffer_size) {
    auto lhandle = ritsuko::hdf5::open_dataset(handle, "lengths");
    if (ritsuko::hdf5::exceeds_integer_limit(lhandle, 64, false)) {
        throw std::runtime_error("expected 'lengths' to have a datatype that fits in a 64-bit unsigned integer");
    }

    auto len = ritsuko::hdf5::get_1d_length(lhandle.getSpace(), false);

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

inline void validate_sparse_indices(const H5::Group& handle, const std::vector<uint64_t>& dimensions, hsize_t num_lengths, hsize_t buffer_size) {
    size_t ndims = dimensions.size();
    std::vector<H5::DataSet> handles;
    handles.reserve(ndims);

    for (size_t d = 0; d < ndims; ++d) {
        auto dname = std::to_string(d);
        auto lhandle = ritsuko::hdf5::open_dataset(handle, dname.c_str());
        if (ritsuko::hdf5::exceeds_integer_limit(lhandle, 64, false)) {
            throw std::runtime_error("expected '" + dname + "' to have a datatype that fits in a 64-bit unsigned integer");
        }
        auto len = ritsuko::hdf5::get_1d_length(lhandle.getSpace(), false);
        if (len != num_lengths) {
            throw std::runtime_error("expected '" + dname + "' to have the same length as 'lengths'");
        }
        handles.push_back(lhandle);
    }

    if (!num_lengths) {
        return;
    }

    std::vector<ritsuko::hdf5::Stream1dNumericDataset<uint64_t> > streams;
    streams.reserve(ndims);
    std::vector<uint64_t> position;
    position.reserve(ndims); 

    for (size_t d = 0; d < ndims; ++d) {
        streams.emplace_back(&(handles[d]), num_lengths, buffer_size);
        position.push_back(streams.back().get());
        if (position.back() >= dimensions[d]) {
            throw std::runtime_error("values in 'indices/" + std::to_string(d) + "' should be less than the corresponding dimension extent");
        }
        streams.back().next();
    }

    for (size_t l = 1; l < num_lengths; ++l) {
        bool greater = false;

        // Going from back to front and checking for increasingness, based on the
        // expectation that the first dimension is the fastest-changing.
        for (size_t rd = ndims; rd > 0; --rd) {
            auto d = rd - 1;
            auto& dstream = streams[d];

            auto nextv = dstream.get();
            if (nextv >= dimensions[d]) {
                throw std::runtime_error("values in 'indices/" + std::to_string(d) + "' should be less than the corresponding dimension extent");
            }
            dstream.next();

            auto& oldv = position[d];
            if (!greater) {
                if (nextv > oldv) {
                    greater = true;
                } else if (nextv < oldv) {
                    throw std::runtime_error("coordinates in 'indices' should be strictly increasing");
                }
            }
            oldv = nextv;
        }

        if (!greater) {
            throw std::runtime_error("duplicate coordinates in 'indices'");
        }
    }
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

    auto dims = validate_dimensions(ghandle);
    auto len = validate_lengths(ghandle, catheight, options.hdf5_buffer_size);

    // Support both dense and sparse cases.
    if (ghandle.exists("indices")) {
        auto ihandle = ritsuko::hdf5::open_group(ghandle, "indices");
        validate_sparse_indices(ihandle, dims, len, options.hdf5_buffer_size);
    } else {
        size_t prod = !dims.empty(); // prod = 0 if dims is empty, otherwise it starts at 1.
        for (auto d : dims) {
            prod *= d;
        }
        if (prod != len) {
            throw std::runtime_error("length of 'lengths' should equal the product of 'dimensions'");
        }
    }

    if (ghandle.exists("names")) {
        internal_array::check_dimnames(ghandle, "names", dims, options);
    }

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate a '" + object_type + "' object at '" + path.string() + "'; " + std::string(e.what()));
}

inline size_t height(const std::filesystem::path& path, const std::string& name, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    auto handle = ritsuko::hdf5::open_file(path / "partitions.h5");
    auto ghandle = handle.openGroup(name);
    auto dhandle = ghandle.openDataSet("dimensions");
    std::array<hsize_t, 2> dims;
    dhandle.read(dims.data(), H5::PredType::NATIVE_HSIZE);
    return dims[0];
}

inline std::vector<size_t> dimensions(const std::filesystem::path& path, const std::string& name, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    auto handle = ritsuko::hdf5::open_file(path / "partitions.h5");
    auto ghandle = handle.openGroup(name);
    auto dhandle = ghandle.openDataSet("dimensions");
    std::vector<hsize_t> dims(2);
    dhandle.read(dims.data(), H5::PredType::NATIVE_HSIZE);
    return std::vector<size_t>(dims.begin(), dims.end());
}

}

}

#endif
