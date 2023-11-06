#ifndef TAKANE_HDF5_DATA_FRAME_HPP
#define TAKANE_HDF5_DATA_FRAME_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include "WrappedOption.hpp"
#include "data_frame.hpp"

#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>

/**
 * @file hdf5_data_frame.hpp
 * @brief Validation for HDF5 data frames.
 */

namespace takane {

/**
 * @namespace takane::hdf5_data_frame
 * @brief Definitions for HDF5 data frames.
 */
namespace hdf5_data_frame {

/**
 * @brief Parameters for validating the HDF5 data frame.
 */
struct Parameters {
    /**
     * @param group Name of the group containing the data frame's contents.
     */
    Parameters(std::string group) : group(std::move(group)) {}

    /**
     * Name of the group containing the data frame's contents.
     */
    std::string group;

    /**
     * Number of rows in the data frame.
     */
    size_t num_rows = 0;

    /**
     * Whether the data frame contains row names.
     */
    bool has_row_names = false;

    /**
     * Details about the expected columns of the data frame, in order.
     * Note that any `factor_levels` inside each entry of `columns` is ignored if a `version` attribute is present on the `group`.
     */
    WrappedOption<std::vector<data_frame::ColumnDetails> > columns;

    /**
     * Buffer size to use when reading values from the HDF5 file.
     */
    hsize_t buffer_size = 10000;

    /**
     * Version of the `data_frame` format.
     * Ignored if a `version` attribute is present on the HDF5 group at `group`.
     */
    int df_version = 2;

    /**
     * Version of the `hdf5_data_frame` format,
     * Ignored if a `version` attribute is present on the HDF5 group at `group`.
     */
    int hdf5_version = 2;
};

/**
 * @cond
 */
inline void validate_row_names(const H5::Group& handle, hsize_t num_rows) try {
    if (!handle.exists("row_names") || handle.childObjType("row_names") != H5O_TYPE_DATASET) {
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

inline void validate_column_names(const H5::Group& ghandle, const Parameters& params) try {
    if (!ghandle.exists("column_names") || ghandle.childObjType("column_names") != H5O_TYPE_DATASET) {
        throw std::runtime_error("expected a 'column_names' dataset");
    }

    auto cnhandle = ghandle.openDataSet("column_names");
    if (cnhandle.getTypeClass() != H5T_STRING) {
        throw std::runtime_error("expected 'column_names' to be a string dataset");
    }

    const auto& columns = *(params.columns);
    size_t num_cols = ritsuko::hdf5::get_1d_length(cnhandle.getSpace(), false);
    if (num_cols != columns.size()) {
        throw std::runtime_error("length of 'column_names' should equal the expected number of columns");
    }

    {
        std::unordered_set<std::string> column_names;
        for (const auto& col : columns) {
            if (col.name.empty()) {
                throw std::runtime_error("column names should not be empty strings");
            }
            if (column_names.find(col.name) != column_names.end()) {
                throw std::runtime_error("duplicated column name '" + col.name + "'");
            }
            column_names.insert(col.name);
        }
    }

    ritsuko::hdf5::load_1d_string_dataset(
        cnhandle, 
        num_cols, 
        params.buffer_size,
        [&](size_t i, const char* p, size_t l) {
            const auto& expected = columns[i].name;
            if (l != expected.size() || strncmp(expected.c_str(), p, l)) {
                throw std::runtime_error("expected name '" + expected + "' but got '" + std::string(p, p + l) + "' for column " + std::to_string(i));
            }
        }
    );

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate the column names for '" + ritsuko::hdf5::get_name(ghandle) + "'; " + std::string(e.what()));
}

// Validation for the older versions where the factors are stored outside of the file.
inline void validate_column_v1_v2(const H5::Group& dhandle, const std::string& dset_name, const data_frame::ColumnDetails& curcol, const Parameters& params) try {
    auto xhandle = ritsuko::hdf5::get_dataset(dhandle, dset_name.c_str());
    if (params.num_rows != ritsuko::hdf5::get_1d_length(xhandle.getSpace(), false)) {
        throw std::runtime_error("expected column to have length equal to the number of rows");
    }

    const char* missing_attr = "missing-value-placeholder";

    if (curcol.type == data_frame::ColumnType::NUMBER) {
        if (xhandle.getTypeClass() != H5T_FLOAT) {
            throw std::runtime_error("expected column to be a floating-point dataset");
        }
        if (params.hdf5_version > 1 && xhandle.attrExists(missing_attr)) {
            ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr);
        }

    } else if (curcol.type == data_frame::ColumnType::BOOLEAN) {
        if (ritsuko::hdf5::exceeds_integer_limit(xhandle, 32, true)) {
            throw std::runtime_error("expected boolean column to use a datatype that is a subset of a 32-bit signed integer");
        }
        if (params.hdf5_version > 1 && xhandle.attrExists(missing_attr)) {
            ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr);
        }

    } else if (curcol.type == data_frame::ColumnType::INTEGER) {
        if (ritsuko::hdf5::exceeds_integer_limit(xhandle, 32, true)) {
            throw std::runtime_error("expected integer column to use a datatype that is a subset of a 32-bit signed integer");
        }
        if (params.hdf5_version > 1 && xhandle.attrExists(missing_attr)) {
            ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr);
        }

    } else if (curcol.type == data_frame::ColumnType::STRING) {
        if (xhandle.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("expected column " + dset_name + " to be a string dataset");
        }

        bool has_missing = xhandle.attrExists(missing_attr);
        std::string missing_value;
        if (has_missing) {
            auto attr = ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr, /* type_class_only = */ true);
            missing_value = ritsuko::hdf5::load_scalar_string_attribute(attr);
        }

        if (curcol.string_format == data_frame::StringFormat::DATE) {
            ritsuko::hdf5::load_1d_string_dataset(
                xhandle, 
                params.num_rows, 
                params.buffer_size,
                [&](size_t, const char* p, size_t l) {
                    std::string x(p, p + l);
                    if (has_missing && missing_value == x) {
                        return;
                    }
                    if (!ritsuko::is_date(p, l)) {
                        throw std::runtime_error("expected a date-formatted string in column (got '" + x + "')");
                    }
                }
            );

        } else if (curcol.string_format == data_frame::StringFormat::DATE_TIME) {
            ritsuko::hdf5::load_1d_string_dataset(
                xhandle, 
                params.num_rows, 
                params.buffer_size,
                [&](size_t, const char* p, size_t l) {
                    std::string x(p, p + l);
                    if (has_missing && missing_value == x) {
                        return;
                    }
                    if (!ritsuko::is_rfc3339(p, l)) {
                        throw std::runtime_error("expected a date/time-formatted string in column (got '" + x + "')");
                    }
                }
            );
        }

    } else if (curcol.type == data_frame::ColumnType::FACTOR) {
        if (params.df_version <= 1) {
            if (xhandle.getTypeClass() != H5T_STRING) {
                throw std::runtime_error("expected column " + dset_name + " to be a string dataset");
            }

            bool has_missing = xhandle.attrExists(missing_attr);
            std::string missing_string;
            if (has_missing) {
                auto attr = ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr, /* type_class_only = */ true);
                missing_string = ritsuko::hdf5::load_scalar_string_attribute(attr);
            }

            const auto& allowed = *(curcol.factor_levels);
            ritsuko::hdf5::load_1d_string_dataset(
                xhandle,
                params.num_rows,
                params.buffer_size,
                [&](hsize_t, const char* p, size_t len) {
                    std::string x(p, p + len);
                    if (has_missing && x == missing_string) {
                        return;
                    } else if (allowed.find(x) == allowed.end()) {
                        throw std::runtime_error("column contains '" + x + "' that is not present in factor levels");
                    }
                }
            );

        } else if (params.df_version > 1) {
            if (ritsuko::hdf5::exceeds_integer_limit(xhandle, 32, true)) {
                throw std::runtime_error("expected factor column to use a datatype that is a subset of a 32-bit signed integer");
            }

            int32_t placeholder = -2147483648;
            bool has_missing = true;
            if (params.hdf5_version > 1) {
                has_missing = xhandle.attrExists(missing_attr);
                if (has_missing) {
                    auto attr = ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr);
                    attr.read(H5::PredType::NATIVE_INT32, &placeholder);
                }
            }

            int32_t num_levels = curcol.factor_levels->size();

            auto block_size = ritsuko::hdf5::pick_1d_block_size(xhandle.getCreatePlist(), params.num_rows, params.buffer_size);
            std::vector<int32_t> buffer(block_size);
            ritsuko::hdf5::iterate_1d_blocks(
                params.num_rows,
                block_size,
                [&](hsize_t, hsize_t len, const H5::DataSpace& memspace, const H5::DataSpace& dataspace) {
                    xhandle.read(buffer.data(), H5::PredType::NATIVE_INT32, memspace, dataspace);
                    for (hsize_t i = 0; i < len; ++i) {
                        if (has_missing && buffer[i] == placeholder) {
                            continue;
                        }
                        if (buffer[i] < 0) {
                            throw std::runtime_error("expected factor indices to be non-negative in column " + dset_name);
                        }
                        if (buffer[i] >= num_levels) {
                            throw std::runtime_error("expected factor indices to be less than the number of levels in column " + dset_name);
                        }
                    }
                }
            );
        }

    } else {
        throw std::runtime_error("no dataset should exist for columns of type 'other'");
    }
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate column at '" + ritsuko::hdf5::get_name(dhandle) + "/" + dset_name + "'; " + std::string(e.what()));
}

// Easier to just create a new function for the newer validators.
inline void validate_column_v3(const H5::Group& dhandle, const std::string& dset_name, const data_frame::ColumnDetails& curcol, const Parameters& params) try { 
    const char* missing_attr = "missing-value-placeholder";

    if (dhandle.childObjType(dset_name) == H5O_TYPE_GROUP) {
        if (curcol.type != data_frame::ColumnType::FACTOR) {
            throw std::runtime_error("only factor columns should be represented as HDF5 groups");
        }

        auto fhandle = dhandle.openGroup(dset_name);
        auto type = ritsuko::hdf5::load_scalar_string_attribute(fhandle, "type");
        if (type != "factor") {
            throw std::runtime_error("expected factor column to have a 'type' attribute set to 'factor'");
        }

        int32_t val = 0;
        if (fhandle.attrExists("ordered")) {
            auto attr = ritsuko::hdf5::get_scalar_attribute(fhandle, "ordered");
            if (ritsuko::hdf5::exceeds_integer_limit(attr, 32, true)) {
                throw std::runtime_error("an 'ordered' attribute on a factor column should have a datatype that fits in a 32-bit signed integer");
            }
            attr.read(H5::PredType::NATIVE_INT32, &val);
        }
        if (val != curcol.factor_ordered) {
            throw std::runtime_error("ordered status of factor is not consistent with the presence of the 'ordered' attribute");
        }

        size_t nlevels = 0;
        {
            auto lhandle = ritsuko::hdf5::get_dataset(fhandle, "levels");
            if (lhandle.getTypeClass() != H5T_STRING) {
                throw std::runtime_error("expected 'levels' to have a string datatype");
            }
            try {
                std::unordered_set<std::string> collected;
                nlevels = ritsuko::hdf5::get_1d_length(lhandle.getSpace(), false);
                ritsuko::hdf5::load_1d_string_dataset(
                    lhandle,
                    nlevels,
                    10000,
                    [&](hsize_t, const char* start, size_t len) {
                        std::string x(start, start+len);
                        if (collected.find(x) != collected.end()) {
                            throw std::runtime_error("detected duplicate level '" + x + "'");
                        }
                        collected.insert(std::move(x));
                    }
                );
            } catch (std::exception& e) {
                throw std::runtime_error("failed to inspect 'levels'; " + std::string(e.what()));
            }
        }

        auto chandle = ritsuko::hdf5::get_dataset(fhandle, "codes");
        if (ritsuko::hdf5::exceeds_integer_limit(chandle, 32, true)) {
            throw std::runtime_error("expected factor column to use a datatype that is a subset of a 32-bit signed integer");
        }
        if (params.num_rows != ritsuko::hdf5::get_1d_length(chandle.getSpace(), false)) {
            throw std::runtime_error("expected column to have length equal to the number of rows");
        }

        bool has_missing = chandle.attrExists(missing_attr);
        int32_t placeholder = 0;
        if (has_missing) {
            auto attr = ritsuko::hdf5::get_missing_placeholder_attribute(chandle, missing_attr);
            attr.read(H5::PredType::NATIVE_INT32, &placeholder);
        }

        // Casting it.
        if (nlevels > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
            throw std::runtime_error("number of levels should not exceed the maximum value of a 32-bit integer");
        }
        int32_t num_levels = nlevels;

        auto block_size = ritsuko::hdf5::pick_1d_block_size(chandle.getCreatePlist(), params.num_rows, params.buffer_size);
        std::vector<int32_t> buffer(block_size);
        ritsuko::hdf5::iterate_1d_blocks(
            params.num_rows,
            block_size,
            [&](hsize_t, hsize_t len, const H5::DataSpace& memspace, const H5::DataSpace& dataspace) {
                chandle.read(buffer.data(), H5::PredType::NATIVE_INT32, memspace, dataspace);
                for (hsize_t i = 0; i < len; ++i) {
                    if (has_missing && buffer[i] == placeholder) {
                        continue;
                    }
                    if (buffer[i] < 0) {
                        throw std::runtime_error("expected factor indices to be non-negative in column " + dset_name);
                    }
                    if (buffer[i] >= num_levels) {
                        throw std::runtime_error("expected factor indices to be less than the number of levels in column " + dset_name);
                    }
                }
            }
        );

    } else {
        auto xhandle = ritsuko::hdf5::get_dataset(dhandle, dset_name.c_str());
        if (params.num_rows != ritsuko::hdf5::get_1d_length(xhandle.getSpace(), false)) {
            throw std::runtime_error("expected column to have length equal to the number of rows");
        }

        if (curcol.type == data_frame::ColumnType::NUMBER) {
            auto type = ritsuko::hdf5::load_scalar_string_attribute(xhandle, "type");
            if (type != "number") {
                throw std::runtime_error("expected number column to have a 'type' attribute set to 'number'");
            }
            if (ritsuko::hdf5::exceeds_float_limit(xhandle, 64)) {
                throw std::runtime_error("expected number column to use a datatype that is a subset of a 64-bit float");
            }
            if (xhandle.attrExists(missing_attr)) {
                ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr);
            }

        } else if (curcol.type == data_frame::ColumnType::BOOLEAN) {
            auto type = ritsuko::hdf5::load_scalar_string_attribute(xhandle, "type");
            if (type != "boolean") {
                throw std::runtime_error("expected boolean column to have a 'type' attribute set to 'boolean'");
            }
            if (ritsuko::hdf5::exceeds_integer_limit(xhandle, 32, true)) {
                throw std::runtime_error("expected boolean column to use a datatype that is a subset of a 32-bit signed integer");
            }
            if (xhandle.attrExists(missing_attr)) {
                ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr);
            }

        } else if (curcol.type == data_frame::ColumnType::INTEGER) {
            auto type = ritsuko::hdf5::load_scalar_string_attribute(xhandle, "type");
            if (type != "integer") {
                throw std::runtime_error("expected integer column to have a 'type' attribute set to 'integer'");
            }
            if (ritsuko::hdf5::exceeds_integer_limit(xhandle, 32, true)) {
                throw std::runtime_error("expected integer column to use a datatype that is a subset of a 32-bit signed integer");
            }
            if (xhandle.attrExists(missing_attr)) {
                ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr);
            }

        } else if (curcol.type == data_frame::ColumnType::STRING) {
            auto type = ritsuko::hdf5::load_scalar_string_attribute(xhandle, "type");
            if (type != "string") {
                throw std::runtime_error("expected string column to have a 'type' attribute set to 'string'");
            }
            if (xhandle.getTypeClass() != H5T_STRING) {
                throw std::runtime_error("expected column " + dset_name + " to be a string dataset");
            }
            bool has_missing = xhandle.attrExists(missing_attr);
            std::string missing_value;
            if (has_missing) {
                auto attr = ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr, /* type_class_only = */ true);
                missing_value = ritsuko::hdf5::load_scalar_string_attribute(attr);
            }

            if (curcol.string_format == data_frame::StringFormat::DATE) {
                auto type = ritsuko::hdf5::load_scalar_string_attribute(xhandle, "format");
                if (type != "date") {
                    throw std::runtime_error("expected date-formatted column to have a 'format' attribute set to 'date'");
                }
                ritsuko::hdf5::load_1d_string_dataset(
                    xhandle, 
                    params.num_rows, 
                    params.buffer_size,
                    [&](size_t, const char* p, size_t l) {
                        std::string x(p, p + l);
                        if (has_missing && missing_value == x) {
                            return;
                        }
                        if (!ritsuko::is_date(p, l)) {
                            throw std::runtime_error("expected a date-formatted string in column (got '" + x + "')");
                        }
                    }
                );

            } else if (curcol.string_format == data_frame::StringFormat::DATE_TIME) {
                auto type = ritsuko::hdf5::load_scalar_string_attribute(xhandle, "format");
                if (type != "date-time") {
                    throw std::runtime_error("expected date/time-formatted column to have a 'format' attribute set to 'date-time'");
                }
                ritsuko::hdf5::load_1d_string_dataset(
                    xhandle, 
                    params.num_rows, 
                    params.buffer_size,
                    [&](size_t, const char* p, size_t l) {
                        std::string x(p, p + l);
                        if (has_missing && missing_value == x) {
                            return;
                        }
                        if (!ritsuko::is_rfc3339(p, l)) {
                            throw std::runtime_error("expected a date/time-formatted string in column (got '" + x + "')");
                        }
                    }
                );

            } else {
                if (xhandle.attrExists("format")) {
                    auto type = ritsuko::hdf5::load_scalar_string_attribute(xhandle, "format");
                    if (type != "none") {
                        throw std::runtime_error("any 'format' attribute on an unformatted string column should be 'none'");
                    }
                }
            }

        } else {
            throw std::runtime_error("no dataset should exist for columns of type 'other' or 'factor'");
        }
    }

} catch (std::exception& e) {
    throw std::runtime_error("failed to validate column at '" + ritsuko::hdf5::get_name(dhandle) + "/" + dset_name + "'; " + std::string(e.what()));
}
/**
 * @endcond
 */

/**
 * Checks if a HDF5 data frame is correctly formatted.
 * An error is raised if the file does not meet the specifications.
 *
 * @param handle Handle to a HDF5 file.
 * @param params Validation parameters.
 */
inline void validate(const H5::H5File& handle, const Parameters& params) {
    if (!handle.exists(params.group) || handle.childObjType(params.group) != H5O_TYPE_GROUP) {
        throw std::runtime_error("expected a '" + params.group + "' group");
    }
    auto ghandle = handle.openGroup(params.group);

    // Inspecting the columns.
    ritsuko::Version version;
    if (ghandle.attrExists("version")) {
        auto vstring = ritsuko::hdf5::load_scalar_string_attribute(ghandle, "version");
        version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
        if (version.major != 1) {
            throw std::runtime_error("unsupported version '" + vstring + "' for the '" + params.group + "' group");
        }
    }

    // Checking the number of rows.
    if (version.major > 0) {
        auto attr = ritsuko::hdf5::get_scalar_attribute(ghandle, "row-count");
        if (ritsuko::hdf5::exceeds_integer_limit(attr, 64, false)) {
            throw std::runtime_error("'row-count' attribute on '" + params.group + "' should have a datatype that fits in a 64-bit unsigned integer");
        }
        uint64_t nrows = 0;
        attr.read(H5::PredType::NATIVE_UINT64, &nrows);
        if (nrows != params.num_rows) {
            throw std::runtime_error("inconsistent number of rows in '" + params.group + "' (expected " + std::to_string(params.num_rows) + ", got " + std::to_string(nrows) + ")");
        }
    }

    // Checking row and column names.
    if (params.has_row_names) {
        validate_row_names(ghandle, params.num_rows);
    }
    validate_column_names(ghandle, params);

    // Finally iterating through the columns.
    if (!ghandle.exists("data") || ghandle.childObjType("data") != H5O_TYPE_GROUP) {
        throw std::runtime_error("expected a '" + params.group + "/data' group");
    }
    auto dhandle = ghandle.openGroup("data");

    const auto& columns = *(params.columns);
    size_t NC = columns.size();
    hsize_t found = 0;
    for (size_t c = 0; c < NC; ++c) {
        const auto& curcol = columns[c];

        std::string dset_name = std::to_string(c);
        if (!dhandle.exists(dset_name)) {
            if (curcol.type == data_frame::ColumnType::OTHER) {
                continue;
            }
        }

        if (version.major > 0) {
            validate_column_v3(dhandle, dset_name, curcol, params);
        } else {
            validate_column_v1_v2(dhandle, dset_name, curcol, params);
        }

        ++found;
    }

    if (found != dhandle.getNumObjs()) {
        throw std::runtime_error("more objects present in the '" + params.group + "/data' group than expected");
    }
}

/**
 * Overload of `hdf5_data_frame::validate()` that accepts a file path.
 *
 * @param path Path to the HDF5 file.
 * @param params Validation parameters.
 */
inline void validate(const char* path, const Parameters& params) {
    H5::H5File handle(path, H5F_ACC_RDONLY);
    validate(handle, params);
}

}

}

#endif
