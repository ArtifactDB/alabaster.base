#ifndef TAKANE_HDF5_FRAME_HPP
#define TAKANE_HDF5_FRAME_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>
#include <filesystem>
#include <unordered_set>

#include "utils_public.hpp"
#include "utils_string.hpp"
#include "utils_factor.hpp"
#include "utils_other.hpp"
#include "utils_json.hpp"

/**
 * @file data_frame.hpp
 * @brief Validation for data frames.
 */

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, const ObjectMetadata&, Options& options);
size_t height(const std::filesystem::path&, const ObjectMetadata&, Options& options);
/**
 * @endcond
 */

namespace data_frame {

/**
 * @cond
 */
inline void validate_row_names(const H5::Group& handle, hsize_t num_rows, const Options& options) try {
    if (handle.childObjType("row_names") != H5O_TYPE_DATASET) {
        throw std::runtime_error("expected a 'row_names' dataset when row names are present");
    }

    auto rnhandle = handle.openDataSet("row_names");
    if (!ritsuko::hdf5::is_utf8_string(rnhandle)) {
        throw std::runtime_error("expected a datatype for 'row_names' that can be represented by a UTF-8 encoded string");
    }

    if (ritsuko::hdf5::get_1d_length(rnhandle.getSpace(), false) != num_rows) {
        throw std::runtime_error("expected 'row_names' to have length equal to the number of rows");
    }
    ritsuko::hdf5::validate_1d_string_dataset(rnhandle, num_rows, options.hdf5_buffer_size);
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate the row names for '" + ritsuko::hdf5::get_name(handle) + "'; " + std::string(e.what()));
}

inline hsize_t validate_column_names(const H5::Group& ghandle, const Options& options) try {
    auto cnhandle = ritsuko::hdf5::open_dataset(ghandle, "column_names");
    if (!ritsuko::hdf5::is_utf8_string(cnhandle)) {
        throw std::runtime_error("expected a datatype for 'column_names' that can be represented by a UTF-8 encoded string");
    }
    
    auto num_cols = ritsuko::hdf5::get_1d_length(cnhandle.getSpace(), false);

    std::unordered_set<std::string> column_names;
    ritsuko::hdf5::Stream1dStringDataset stream(&cnhandle, num_cols, options.hdf5_buffer_size);
    for (size_t c = 0; c < num_cols; ++c, stream.next()) {
        auto x = stream.steal();
        if (x.empty()) {
            throw std::runtime_error("column names should not be empty strings");
        }
        if (column_names.find(x) != column_names.end()) {
            throw std::runtime_error("duplicated column name '" + x + "'");
        }
        column_names.insert(std::move(x));
    }

    return num_cols;

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate the column names for '" + ritsuko::hdf5::get_name(ghandle) + "'; " + std::string(e.what()));
}

inline void validate_column(const H5::Group& dhandle, const std::string& dset_name, hsize_t num_rows, const Options& options) try { 
    auto dtype = dhandle.childObjType(dset_name);
    if (dtype == H5O_TYPE_GROUP) {
        auto fhandle = dhandle.openGroup(dset_name);
        auto type = ritsuko::hdf5::open_and_load_scalar_string_attribute(fhandle, "type");
        if (type != "factor") {
            throw std::runtime_error("expected HDF5 groups to have a 'type' attribute set to 'factor'");
        }

        internal_factor::check_ordered_attribute(fhandle);

        auto num_levels = internal_factor::validate_factor_levels(fhandle, "levels", options.hdf5_buffer_size);
        auto num_codes = internal_factor::validate_factor_codes(fhandle, "codes", num_levels, options.hdf5_buffer_size);
        if (num_codes != num_rows) {
            throw std::runtime_error("expected column to have length equal to the number of rows");
        }

    } else if (dtype == H5O_TYPE_DATASET) {
        auto xhandle = dhandle.openDataSet(dset_name);
        if (num_rows != ritsuko::hdf5::get_1d_length(xhandle.getSpace(), false)) {
            throw std::runtime_error("expected column to have length equal to the number of rows");
        }

        const char* missing_attr_name = "missing-value-placeholder";

        auto type = ritsuko::hdf5::open_and_load_scalar_string_attribute(xhandle, "type");
        if (type == "string") {
            if (!ritsuko::hdf5::is_utf8_string(xhandle)) {
                throw std::runtime_error("expected a datatype for '" + dset_name + "' that can be represented by a UTF-8 encoded string");
            }
            auto missingness = ritsuko::hdf5::open_and_load_optional_string_missing_placeholder(xhandle, missing_attr_name);
            std::string format = internal_string::fetch_format_attribute(xhandle);
            internal_string::validate_string_format(xhandle, num_rows, format, missingness.first, missingness.second, options.hdf5_buffer_size);

        } else {
            if (type == "integer") {
                if (ritsuko::hdf5::exceeds_integer_limit(xhandle, 32, true)) {
                    throw std::runtime_error("expected integer column to use a datatype that is a subset of a 32-bit signed integer");
                }
            } else if (type == "boolean") {
                if (ritsuko::hdf5::exceeds_integer_limit(xhandle, 32, true)) {
                    throw std::runtime_error("expected boolean column to use a datatype that is a subset of a 32-bit signed integer");
                }
            } else if (type == "number") {
                if (ritsuko::hdf5::exceeds_float_limit(xhandle, 64)) {
                    throw std::runtime_error("expected number column to use a datatype that is a subset of a 64-bit float");
                }
            } else {
                throw std::runtime_error("unknown column type '" + type + "'");
            }

            if (xhandle.attrExists(missing_attr_name)) {
                auto ahandle = xhandle.openAttribute(missing_attr_name);
                ritsuko::hdf5::check_missing_placeholder_attribute(xhandle, ahandle);
            }
        }

    } else {
        throw std::runtime_error("unknown HDF5 object type");
    }

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate column at '" + ritsuko::hdf5::get_name(dhandle) + "/" + dset_name + "'; " + std::string(e.what()));
}
/**
 * @endcond
 */

/**
 * @param path Path to the directory containing the data frame.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, Options& options) {
    const auto& vstring = internal_json::extract_version_for_type(metadata.other, "data_frame");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version '" + vstring + "'");
    }

    auto handle = ritsuko::hdf5::open_file(path / "basic_columns.h5");
    auto ghandle = ritsuko::hdf5::open_group(handle, "data_frame");

    // Checking the number of rows.
    auto attr = ritsuko::hdf5::open_scalar_attribute(ghandle, "row-count");
    if (ritsuko::hdf5::exceeds_integer_limit(attr, 64, false)) {
        throw std::runtime_error("'row-count' attribute should have a datatype that fits in a 64-bit unsigned integer");
    }
    uint64_t num_rows = 0;
    attr.read(H5::PredType::NATIVE_UINT64, &num_rows);

    // Checking row and column names.
    if (ghandle.exists("row_names")) {
        validate_row_names(ghandle, num_rows, options);
    }
    size_t NC = validate_column_names(ghandle, options);

    // Finally iterating through the columns.
    auto dhandle = ritsuko::hdf5::open_group(ghandle, "data");

    hsize_t num_basic = 0;
    auto other_dir = path / "other_columns";

    for (size_t c = 0; c < NC; ++c) {
        std::string dset_name = std::to_string(c);

        if (!dhandle.exists(dset_name)) {
            auto opath = other_dir / dset_name;
            auto ometa = read_object_metadata(opath);
            try {
                ::takane::validate(opath, ometa, options);
            } catch (std::exception& e) {
                throw std::runtime_error("failed to validate 'other' column " + dset_name + "; " + std::string(e.what()));
            }
            if (::takane::height(opath, ometa, options) != num_rows) {
                throw std::runtime_error("height of column " + dset_name + " of class '" + ometa.type + "' is not the same as the number of rows");
            }

        } else {
            validate_column(dhandle, dset_name, num_rows, options);
            ++num_basic;
        }
    }

    if (std::filesystem::exists(other_dir)) {
        if (internal_other::count_directory_entries(other_dir) != NC - num_basic) {
            throw std::runtime_error("more objects than expected inside the 'other_columns' directory");
        }
    }

    if (num_basic != dhandle.getNumObjs()) {
        throw std::runtime_error("more objects present in the 'data_frame/data' group than expected");
    }

    internal_other::validate_mcols(path, "column_annotations", NC, options);
    internal_other::validate_metadata(path, "other_annotations", options);
}

/**
 * @param path Path to a directory containing a data frame.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return The number of rows.
 */
inline size_t height(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    // Assume it's all valid already.
    auto handle = ritsuko::hdf5::open_file(path / "basic_columns.h5");
    auto ghandle = handle.openGroup("data_frame");
    return ritsuko::hdf5::load_scalar_numeric_attribute<uint64_t>(ghandle.openAttribute("row-count"));
}

/**
 * @param path Path to a directory containing a data frame.
 * @param metadata Metadata for the object, typically read from its `OBJECT` file.
 * @param options Validation options.
 * @return A vector of length 2 containing the number of rows and columns in the data frame.
 */
inline std::vector<size_t> dimensions(const std::filesystem::path& path, [[maybe_unused]] const ObjectMetadata& metadata, [[maybe_unused]] Options& options) {
    // Assume it's all valid already.
    auto handle = ritsuko::hdf5::open_file(path / "basic_columns.h5");
    auto ghandle = handle.openGroup("data_frame");
    std::vector<size_t> output(2);
    output[0] = ritsuko::hdf5::load_scalar_numeric_attribute<uint64_t>(ghandle.openAttribute("row-count"));
    output[1] = ritsuko::hdf5::get_1d_length(ghandle.openDataSet("column_names"), false);
    return output;
}

}

}

#endif
