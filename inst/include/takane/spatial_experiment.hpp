#ifndef TAKANE_SPATIAL_EXPERIMENT_HPP
#define TAKANE_SPATIAL_EXPERIMENT_HPP

#include "ritsuko/hdf5/hdf5.hpp"

#include "single_cell_experiment.hpp"
#include "utils_factor.hpp"
#include "utils_public.hpp"
#include "utils_other.hpp"
#include "utils_files.hpp"

#include <filesystem>
#include <stdexcept>
#include <unordered_set>
#include <string>
#include <vector>
#include <cmath>

/**
 * @file spatial_experiment.hpp
 * @brief Validation for spatial experiments.
 */

namespace takane {

/**
 * @cond
 */
bool derived_from(const std::string&, const std::string&, const Options& options);
void validate(const std::filesystem::path&, const ObjectMetadata&, Options& options);
bool satisfies_interface(const std::string&, const std::string&, const Options& options);
/**
 * @endcond
 */

/**
 * @namespace takane::spatial_experiment
 * @brief Definitions for spatial experiments.
 */
namespace spatial_experiment {

/**
 * @cond
 */
namespace internal {

inline void validate_coordinates(const std::filesystem::path& path, size_t ncols, Options& options) {
    auto coord_path = path / "coordinates";
    auto coord_meta = read_object_metadata(coord_path);
    if (!derived_from(coord_meta.type, "dense_array", options)) {
        throw std::runtime_error("'coordinates' should be a dense array");
    }

    // Validating the coordinates; currently these must be a dense array of
    // points, but could also be polygons/hulls in the future.
    try {
        ::takane::validate(coord_path, coord_meta, options);
    } catch (std::exception& e) {
        throw std::runtime_error("failed to validate 'coordinates'; " + std::string(e.what()));
    }

    auto cdims = ::takane::dimensions(coord_path, coord_meta, options);
    if (cdims.size() != 2) {
        throw std::runtime_error("'coordinates' should be a 2-dimensional dense array");
    } else if (cdims[1] != 2 && cdims[1] != 3) {
        throw std::runtime_error("'coordinates' should have 2 or 3 columns");
    } else if (cdims[0] != ncols) {
        throw std::runtime_error("number of rows in 'coordinates' should equal the number of columns in the 'spatial_experiment'");
    }

    // Checking that the values are numeric.
    auto handle = ritsuko::hdf5::open_file(coord_path / "array.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "dense_array");
    auto dhandle = ritsuko::hdf5::open_dataset(ghandle, "data");
    auto dclass = dhandle.getTypeClass();
    if (dclass != H5T_INTEGER && dclass != H5T_FLOAT) {
        throw std::runtime_error("values in 'coordinates' should be numeric");
    }
}

inline void validate_image(const std::filesystem::path& path, size_t i, const std::string& format, Options& options, const ritsuko::Version& version) {
    auto ipath = path / std::to_string(i);

    if (format == "PNG") {
        ipath += ".png";
        // Magic number from http://www.libpng.org/pub/png/spec/1.2/png-1.2-pdg.html#PNG-file-signature
        std::array<unsigned char, 8> expected { 137, 80, 78, 71, 13, 10, 26, 10 };
        internal_files::check_signature(ipath, expected.data(), expected.size(), "PNG");

    } else if (format == "TIFF") {
        ipath += ".tif";
        std::array<unsigned char, 4> observed;
        internal_files::extract_signature(ipath, observed.data(), observed.size());
        // Magic numbers from https://en.wikipedia.org/wiki/Magic_number_(programming)
        std::array<unsigned char, 4> iisig = { 0x49, 0x49, 0x2A, 0x00 };
        std::array<unsigned char, 4> mmsig = { 0x4D, 0x4D, 0x00, 0x2A };
        if (observed != iisig && observed != mmsig) {
            throw std::runtime_error("incorrect TIFF file signature for '" + ipath.string() + "'");
        }

    } else if (format == "OTHER" && version.ge(1, 1, 0)) {
        auto imeta = read_object_metadata(ipath);
        if (!satisfies_interface(imeta.type, "IMAGE", options)) {
            throw std::runtime_error("object in '" + ipath.string() + "' should satisfy the 'IMAGE' interface");
        }
        ::takane::validate(ipath, imeta, options);

    } else {
        throw std::runtime_error("image format '" + format + "' is not currently supported");
    }
}

inline void validate_images(const std::filesystem::path& path, size_t ncols, Options& options, const ritsuko::Version& version) {
    auto image_dir = path / "images";
    if (!std::filesystem::exists(image_dir) && version.ge(1, 2, 0)) {
        // No images at all, which is permitted.
        return;
    }

    auto mappath = image_dir / "mapping.h5";
    auto ihandle = ritsuko::hdf5::open_file(mappath);
    auto ghandle = ritsuko::hdf5::open_group(ihandle, "spatial_experiment");

    std::vector<std::string> image_formats;
    try {
        struct SampleMapMessenger {
            static std::string level() { return "sample name"; }
            static std::string levels() { return "sample names"; }
            static std::string codes() { return "sample assignments"; }
        };

        auto num_samples = internal_factor::validate_factor_levels<SampleMapMessenger>(ghandle, "sample_names", options.hdf5_buffer_size);
        auto num_codes = internal_factor::validate_factor_codes<SampleMapMessenger>(ghandle, "column_samples", num_samples, options.hdf5_buffer_size, true);
        if (num_codes != ncols) {
            throw std::runtime_error("length of 'column_samples' should equal the number of columns in the spatial experiment");
        }

        // Scanning through the image information.
        auto sample_handle = ritsuko::hdf5::open_dataset(ghandle, "image_samples");
        if (ritsuko::hdf5::exceeds_integer_limit(sample_handle, 64, false)) {
            throw std::runtime_error("expected a datatype for 'image_samples' that fits in a 64-bit unsigned integer");
        }
        auto num_images = ritsuko::hdf5::get_1d_length(sample_handle.getSpace(), false);

        auto id_handle = ritsuko::hdf5::open_dataset(ghandle, "image_ids");
        if (!ritsuko::hdf5::is_utf8_string(id_handle)) {
            throw std::runtime_error("expected 'image_ids' to have a datatype that can be represented by a UTF-8 encoded string");
        }
        if (ritsuko::hdf5::get_1d_length(id_handle.getSpace(), false) != num_images) {
            throw std::runtime_error("expected 'image_ids' to have the same length as 'image_samples'");
        }

        auto scale_handle = ritsuko::hdf5::open_dataset(ghandle, "image_scale_factors");
        if (ritsuko::hdf5::exceeds_float_limit(scale_handle, 64)) {
            throw std::runtime_error("expected a datatype for 'image_scale_factors' that fits in a 64-bit float");
        }
        if (ritsuko::hdf5::get_1d_length(scale_handle.getSpace(), false) != num_images) {
            throw std::runtime_error("expected 'image_scale_factors' to have the same length as 'image_samples'");
        }

        auto format_handle = ritsuko::hdf5::open_dataset(ghandle, "image_formats");
        if (!ritsuko::hdf5::is_utf8_string(format_handle)) {
            throw std::runtime_error("expected 'image_formats' to have a datatype that can be represented by a UTF-8 encoded string");
        }
        if (ritsuko::hdf5::get_1d_length(format_handle.getSpace(), false) != num_images) {
            throw std::runtime_error("expected 'image_formats' to have the same length as 'image_samples'");
        }

        ritsuko::hdf5::Stream1dNumericDataset<uint64_t> sample_stream(&sample_handle, num_images, options.hdf5_buffer_size);
        ritsuko::hdf5::Stream1dStringDataset id_stream(&id_handle, num_images, options.hdf5_buffer_size);
        ritsuko::hdf5::Stream1dNumericDataset<double> scale_stream(&scale_handle, num_images, options.hdf5_buffer_size);
        ritsuko::hdf5::Stream1dStringDataset format_stream(&format_handle, num_images, options.hdf5_buffer_size);
        std::vector<std::unordered_set<std::string> > collected(num_samples);
        image_formats.reserve(num_images);

        for (hsize_t i = 0; i < num_images; ++i) {
            auto sample = sample_stream.get();
            if (sample >= num_samples) {
                throw std::runtime_error("entries of 'image_samples' should be less than the number of samples");
            }
            sample_stream.next();

            auto& present = collected[sample];
            auto id = id_stream.steal();
            if (present.find(id) != present.end()) {
                throw std::runtime_error("'image_ids' contains duplicated image IDs for the same sample + ('" + id + "')");
            }
            present.insert(std::move(id));
            id_stream.next();

            auto sc = scale_stream.get();
            if (!std::isfinite(sc) || sc <= 0) {
                throw std::runtime_error("entries of 'image_scale_factors' should be finite and positive");
            }
            scale_stream.next();

            auto fmt = format_stream.steal();
            image_formats.push_back(std::move(fmt));
            format_stream.next();
        }

        for (const auto& x : collected) {
            if (x.empty()) {
                throw std::runtime_error("each sample should map to one or more images in 'image_samples'");
            }
        }

    } catch (std::exception& e) {
        throw std::runtime_error("failed to validate '" + mappath.string() + "'; " + std::string(e.what()));
    }

    // Now validating the images themselves.
    size_t num_images = image_formats.size();
    for (size_t i = 0; i < num_images; ++i) {
        validate_image(image_dir, i, image_formats[i], options, version);
    }

    size_t num_dir_obj = internal_other::count_directory_entries(image_dir);
    if (num_dir_obj - 1 != num_images) { // -1 to account for the mapping.h5 file itself.
        throw std::runtime_error("more objects than expected inside the 'images' subdirectory");
    }
}

}
/**
 * @endcond
 */

/**
 * @param path Path to the directory containing the spatial experiment.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    ::takane::single_cell_experiment::validate(path, metadata, options);

    const std::string& vstring = internal_json::extract_version_for_type(metadata.other, "spatial_experiment");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version string '" + vstring + "'");
    }

    auto dims = ::takane::summarized_experiment::dimensions(path, metadata, options);
    internal::validate_coordinates(path, dims[1], options);
    internal::validate_images(path, dims[1], options, version);
}

}

}

#endif
