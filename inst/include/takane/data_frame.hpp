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
#include "utils_hdf5.hpp"
#include "utils_other.hpp"

/**
 * @file data_frame.hpp
 * @brief Validation for data frames.
 */

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, const Options&);
size_t height(const std::filesystem::path&, const Options&);
/**
 * @endcond
 */

namespace data_frame {

/**
 * @cond
 */
inline void validate_row_names(const H5::Group& handle, hsize_t num_rows) try {
    if (handle.childObjType("row_names") != H5O_TYPE_DATASET) {
        throw std::runtime_error("expected a 'row_names' dataset when row names are present");
    }
    auto rnhandle = handle.openDataSet("row_names");
    if (rnhandle.getTypeClass() != H5T_STRING) {
        throw std::runtime_error("expected 'row_names' to be a string dataset");
    }
    if (ritsuko::hdf5::get_1d_length(rnhandle.getSpace(), false) != num_rows) {
        throw std::runtime_error("expected 'row_names' to have length equal to the number of rows");
    }
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate the row names for '" + ritsuko::hdf5::get_name(handle) + "'; " + std::string(e.what()));
}

inline hsize_t validate_column_names(const H5::Group& ghandle, const Options& options) try {
    if (!ghandle.exists("column_names") || ghandle.childObjType("column_names") != H5O_TYPE_DATASET) {
        throw std::runtime_error("expected a 'column_names' dataset");
    }

    auto cnhandle = ghandle.openDataSet("column_names");
    if (cnhandle.getTypeClass() != H5T_STRING) {
        throw std::runtime_error("expected 'column_names' to be a string dataset");
    }
    
    auto num_cols = ritsuko::hdf5::get_1d_length(cnhandle.getSpace(), false);

    std::unordered_set<std::string> column_names;
    ritsuko::hdf5::load_1d_string_dataset(
        cnhandle, 
        num_cols, 
        options.hdf5_buffer_size,
        [&](size_t, const char* p, size_t l) {
            if (l == 0) {
                throw std::runtime_error("column names should not be empty strings");
            }
            std::string col_name(p, p + l);
            if (column_names.find(col_name) != column_names.end()) {
                throw std::runtime_error("duplicated column name '" + col_name + "'");
            }
            column_names.insert(std::move(col_name));
        }
    );

    return num_cols;

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate the column names for '" + ritsuko::hdf5::get_name(ghandle) + "'; " + std::string(e.what()));
}

inline void validate_column(const H5::Group& dhandle, const std::string& dset_name, hsize_t num_rows, const Options& options) try { 
    if (dhandle.childObjType(dset_name) == H5O_TYPE_GROUP) {
        auto fhandle = dhandle.openGroup(dset_name);
        auto type = ritsuko::hdf5::load_scalar_string_attribute(fhandle, "type");
        if (type != "factor") {
            throw std::runtime_error("expected HDF5 groups to have a 'type' attribute set to 'factor'");
        }

        if (fhandle.attrExists("ordered")) {
            auto attr = ritsuko::hdf5::get_scalar_attribute(fhandle, "ordered");
            if (ritsuko::hdf5::exceeds_integer_limit(attr, 32, true)) {
                throw std::runtime_error("an 'ordered' attribute on a factor column should have a datatype that fits in a 32-bit signed integer");
            }
        }

        auto num_levels = internal_hdf5::validate_factor_levels(fhandle, "levels", options.hdf5_buffer_size);
        auto num_codes = internal_hdf5::validate_factor_codes(fhandle, "codes", num_levels, options.hdf5_buffer_size);
        if (num_codes != num_rows) {
            throw std::runtime_error("expected column to have length equal to the number of rows");
        }


    } else {
        auto xhandle = ritsuko::hdf5::get_dataset(dhandle, dset_name.c_str());
        if (num_rows != ritsuko::hdf5::get_1d_length(xhandle.getSpace(), false)) {
            throw std::runtime_error("expected column to have length equal to the number of rows");
        }

        const char* missing_attr_name = "missing-value-placeholder";
        bool has_missing = xhandle.attrExists(missing_attr_name);

        auto type = ritsuko::hdf5::load_scalar_string_attribute(xhandle, "type");
        if (type == "string") {
            if (xhandle.getTypeClass() != H5T_STRING) {
                throw std::runtime_error("expected column " + dset_name + " to be a string dataset");
            }

            std::string missing_value;
            if (has_missing) {
                auto missing_attr = ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr_name, /* type_class_only = */ true);
                missing_value = ritsuko::hdf5::load_scalar_string_attribute(missing_attr);
            }

            if (xhandle.attrExists("format")) {
                auto format = ritsuko::hdf5::load_scalar_string_attribute(xhandle, "format");
                internal_hdf5::validate_string_format(xhandle, num_rows, format, has_missing, missing_value, options.hdf5_buffer_size);
            }

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

            if (has_missing) {
                ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr_name);
            }
        }
    }

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate column at '" + ritsuko::hdf5::get_name(dhandle) + "/" + dset_name + "'; " + std::string(e.what()));
}
/**
 * @endcond
 */

/**
 * @param path Path to the directory containing the data frame.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const Options& options) {
    auto h5path = path / "basic_columns.h5";

    H5::H5File handle(h5path, H5F_ACC_RDONLY);
    if (!handle.exists("data_frame") || handle.childObjType("data_frame") != H5O_TYPE_GROUP) {
        throw std::runtime_error("expected a 'data_frame' group");
    }
    auto ghandle = handle.openGroup("data_frame");

    auto vstring = ritsuko::hdf5::load_scalar_string_attribute(ghandle, "version");
    auto version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
    if (version.major != 1) {
        throw std::runtime_error("unsupported version '" + vstring + "'");
    }

    // Checking the number of rows.
    auto attr = ritsuko::hdf5::get_scalar_attribute(ghandle, "row-count");
    if (ritsuko::hdf5::exceeds_integer_limit(attr, 64, false)) {
        throw std::runtime_error("'row-count' attribute should have a datatype that fits in a 64-bit unsigned integer");
    }
    uint64_t num_rows = 0;
    attr.read(H5::PredType::NATIVE_UINT64, &num_rows);

    // Checking row and column names.
    if (ghandle.exists("row_names")) {
        validate_row_names(ghandle, num_rows);
    }
    size_t NC = validate_column_names(ghandle, options);

    // Finally iterating through the columns.
    if (!ghandle.exists("data") || ghandle.childObjType("data") != H5O_TYPE_GROUP) {
        throw std::runtime_error("expected a 'data_frame/data' group");
    }
    auto dhandle = ghandle.openGroup("data");

    hsize_t found = 0;
    for (size_t c = 0; c < NC; ++c) {
        std::string dset_name = std::to_string(c);

        if (!dhandle.exists(dset_name)) {
            auto opath = path / "other_columns" / dset_name;
            try {
                ::takane::validate(opath, options);
            } catch (std::exception& e) {
                throw std::runtime_error("failed to validate 'other' column " + dset_name + "; " + std::string(e.what()));
            }
            if (::takane::height(opath, options) != num_rows) {
                throw std::runtime_error("height of column " + dset_name + " of class '" + read_object_type(opath) + "' is not the same as the number of rows");
            }

        } else {
            validate_column(dhandle, dset_name, num_rows, options);
            ++found;
        }
    }

    if (found != dhandle.getNumObjs()) {
        throw std::runtime_error("more objects present in the 'data_frame/data' group than expected");
    }

    // Checking the metadata.
    try {
        internal_other::validate_mcols(path / "column_annotations", NC, options);
    } catch (std::exception& e) {
        throw std::runtime_error("failed to validate 'column_annotations'; " + std::string(e.what()));
    }

    try {
        internal_other::validate_metadata(path / "other_annotations", options);
    } catch (std::exception& e) {
        throw std::runtime_error("failed to validate 'other_annotations'; " + std::string(e.what()));
    }
}

/**
 * @param path Path to a directory containing a data frame.
 * @param options Validation options, mostly for input performance.
 * @return The number of rows.
 */
inline size_t height(const std::filesystem::path& path, const Options&) {
    auto h5path = path / "basic_columns.h5";

    // Assume it's all valid already.
    H5::H5File handle(h5path, H5F_ACC_RDONLY);
    auto ghandle = handle.openGroup("data_frame");
    auto attr = ritsuko::hdf5::get_scalar_attribute(ghandle, "row-count");
    uint64_t num_rows = 0;
    attr.read(H5::PredType::NATIVE_UINT64, &num_rows);
    return num_rows;
}

}

}

#endif
