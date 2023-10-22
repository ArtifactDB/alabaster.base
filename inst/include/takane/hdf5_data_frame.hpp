#ifndef TAKANE_HDF5_DATA_FRAME_HPP
#define TAKANE_HDF5_DATA_FRAME_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

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

namespace data_frame {

/**
 * Checks if a HDF5 data frame is correctly formatted.
 * An error is raised if the file does not meet the specifications.
 *
 * @param handle Handle to the HDF5 group containing the data frame's contents.
 * @param num_rows Number of rows in the data frame.
 * @param has_row_names Whether the data frame contains row names.
 * @param columns Details about the expected columns of the data frame, in order.
 * @param df_version Version of the `data_frame` format.
 * @param hdf5_version Version of the `hdf5_data_frame` format.
 */
inline void validate_hdf5(const H5::Group& handle, hsize_t num_rows, bool has_row_names, const std::vector<ColumnDetails>& columns, int df_version = 2, int hdf5_version = 2) {
    constexpr hsize_t buffer_size = 10000;
    const char* missing_attr = "missing-value-placeholder";

    // Checking the row names.
    if (has_row_names) {
        if (!handle.exists("row_names") || handle.childObjType("row_names") != H5O_TYPE_DATASET) {
            throw std::runtime_error("expected a 'row_names' dataset when row names is present");
        }

        auto rnhandle = handle.openDataSet("row_names");
        if (rnhandle.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("expected 'row_names' to be a string dataset");
        }

        if (ritsuko::hdf5::get_1d_length(rnhandle.getSpace(), false, "row_names") != num_rows) {
            throw std::runtime_error("expected 'row_names' to have length equal to the number of rows");
        }
    }

    // Checking the column names.
    {
        if (!handle.exists("column_names") || handle.childObjType("column_names") != H5O_TYPE_DATASET) {
            throw std::runtime_error("expected a 'column_names' dataset");
        }

        auto cnhandle = handle.openDataSet("column_names");
        if (cnhandle.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("expected 'column_names' to be a string dataset");
        }

        size_t ncols = ritsuko::hdf5::get_1d_length(cnhandle.getSpace(), false, "column_names");
        if (ncols != columns.size()) {
            throw std::runtime_error("length of 'column_names' should equal the expected number of columns");
        }

        ritsuko::hdf5::load_1d_string_dataset(
            cnhandle, 
            ncols, 
            buffer_size,
            [&](size_t i, const char* p, size_t l) {
                const auto& expected = columns[i].name;
                if (l != expected.size() || strncmp(expected.c_str(), p, l)) {
                    throw std::runtime_error("expected name '" + expected + "' but got '" + std::string(p, p + l) + "' for column " + std::to_string(i));
                }
            }
        );

        std::unordered_set<std::string> column_names;
        for (const auto& col : columns) {
            if (column_names.find(col.name) != column_names.end()) {
                throw std::runtime_error("duplicated column name '" + col.name + "'");
            }
            column_names.insert(col.name);
        }
    }

    if (!handle.exists("data") || handle.childObjType("data") != H5O_TYPE_GROUP) {
        throw std::runtime_error("expected a 'data' group");
    }
    auto dhandle = handle.openGroup("data");

    // Checking each column individually.
    size_t NC = columns.size();
    hsize_t found = 0;
    for (size_t c = 0; c < NC; ++c) {
        const auto& curcol = columns[c];

        std::string dset_name = std::to_string(c);
        if (!handle.exists(dset_name)) {
            if (curcol.type == data_frame::ColumnType::OTHER) {
                continue;
            }
        }

        auto xhandle = ritsuko::hdf5::get_dataset(dhandle, dset_name.c_str());
        if (num_rows != ritsuko::hdf5::get_1d_length(xhandle.getSpace(), false, dset_name.c_str())) {
            throw std::runtime_error("expected column " + dset_name + " to have length equal to the number of rows");
        }

        if (curcol.type == ColumnType::NUMBER) {
            if (xhandle.getTypeClass() != H5T_FLOAT) {
                throw std::runtime_error("expected column " + dset_name + " to be a floating-point dataset");
            }
            if (hdf5_version > 1 && xhandle.attrExists(missing_attr)) {
                ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr, dset_name.c_str());
            }

        } else if (curcol.type == data_frame::ColumnType::BOOLEAN || curcol.type == data_frame::ColumnType::INTEGER) {
            if (xhandle.getTypeClass() != H5T_INTEGER) {
                throw std::runtime_error("expected column " + dset_name + " to be an integer dataset");
            }
            ritsuko::hdf5::forbid_large_integers(xhandle, 32, dset_name.c_str());
            if (hdf5_version > 1 && xhandle.attrExists(missing_attr)) {
                ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr, dset_name.c_str());
            }

        } else if (curcol.type == ColumnType::STRING) {
            if (xhandle.getTypeClass() != H5T_STRING) {
                throw std::runtime_error("expected column " + dset_name + " to be a string dataset");
            }

            bool has_missing = xhandle.attrExists(missing_attr);
            std::string missing_value;
            if (has_missing) {
                auto attr = ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr, dset_name.c_str(), /* type_class_only = */ true);
                missing_value = ritsuko::hdf5::load_scalar_string_attribute(attr, missing_attr, dset_name.c_str());
            }

            if (curcol.format == StringFormat::DATE) {
                ritsuko::hdf5::load_1d_string_dataset(
                    xhandle, 
                    num_rows, 
                    buffer_size,
                    [&](size_t, const char* p, size_t l) {
                        std::string x(p, p + l);
                        if (has_missing && missing_value == x) {
                            return;
                        }
                        if (!ritsuko::is_date(p, l)) {
                            throw std::runtime_error("expected a date-formatted string in column " + dset_name + " (" + x + ")");
                        }
                    }
                );

            } else if (curcol.format == StringFormat::DATE_TIME) {
                ritsuko::hdf5::load_1d_string_dataset(
                    xhandle, 
                    num_rows, 
                    buffer_size,
                    [&](size_t, const char* p, size_t l) {
                        std::string x(p, p + l);
                        if (has_missing && missing_value == x) {
                            return;
                        }
                        if (!ritsuko::is_rfc3339(p, l)) {
                            throw std::runtime_error("expected a date/time-formatted string in column " + dset_name + " (" + x + ")");
                        }
                    }
                );
            }

        } else if (curcol.type == data_frame::ColumnType::FACTOR) {
            if (df_version <= 1) {
                if (xhandle.getTypeClass() != H5T_STRING) {
                    throw std::runtime_error("expected column " + dset_name + " to be a string dataset");
                }

                bool has_missing = xhandle.attrExists(missing_attr);
                std::string missing_string;
                if (has_missing) {
                    auto attr = ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr, dset_name.c_str(), /* type_class_only = */ true);
                    missing_string = ritsuko::hdf5::load_scalar_string_attribute(attr, missing_attr, dset_name.c_str());
                }

                const auto& allowed = curcol.factor_levels;
                ritsuko::hdf5::load_1d_string_dataset(
                    xhandle,
                    num_rows,
                    buffer_size,
                    [&](hsize_t, const char* p, size_t len) {
                        std::string x(p, p + len);
                        if (has_missing && x == missing_string) {
                            return;
                        } else if (allowed.find(x) == allowed.end()) {
                            throw std::runtime_error("column " + dset_name + " contains '" + x + "' that is not present in factor levels");
                        }
                    }
                );

            } else if (df_version > 1) {
                if (xhandle.getTypeClass() != H5T_INTEGER) {
                    throw std::runtime_error("expected column " + dset_name + " to be an integer dataset");
                }
                ritsuko::hdf5::forbid_large_integers(xhandle, 32, dset_name.c_str());

                int32_t placeholder = -2147483648;
                bool has_missing = true;
                if (hdf5_version > 1) {
                    has_missing = xhandle.attrExists(missing_attr);
                    if (has_missing) {
                        auto attr = ritsuko::hdf5::get_missing_placeholder_attribute(xhandle, missing_attr, dset_name.c_str());
                        attr.read(H5::PredType::NATIVE_INT32, &placeholder);
                    }
                }

                int32_t num_levels = curcol.factor_levels.size();

                auto block_size = ritsuko::hdf5::pick_1d_block_size(xhandle.getCreatePlist(), num_rows, buffer_size);
                std::vector<int32_t> buffer(block_size);
                ritsuko::hdf5::iterate_1d_blocks(
                    num_rows,
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
                                throw std::runtime_error("expected factor indices to less than the number of levels in column " + dset_name);
                            }
                        }
                    }
                );
            }

        } else {
            throw std::runtime_error("expected no dataset for 'other' column " + dset_name);
        }

        ++found;
    }

    if (found != dhandle.getNumObjs()) {
        throw std::runtime_error("more objects present in the 'data' group than expected");
    }
}

/**
 * Checks if a HDF5 data frame is correctly formatted.
 * An error is raised if the file does not meet the specifications.
 *
 * @param path Path to the HDF5 file.
 * @param name Name of the group containing the data frame contents.
 * @param num_rows Number of rows in the data frame.
 * @param has_row_names Whether the data frame contains row names.
 * @param columns Details about the expected columns of the data frame, in order.
 * @param df_version Version of the `data_frame` format.
 * @param hdf5_version Version of the `hdf5_data_frame` format.
 */
inline void validate_hdf5(const std::string& path, const std::string& name, hsize_t num_rows, bool has_row_names, const std::vector<ColumnDetails>& columns, int df_version = 2, int hdf5_version = 2) {
    H5::H5File handle(path, H5F_ACC_RDONLY);
    auto ghandle = handle.openGroup(name);
    validate_hdf5(ghandle, num_rows, has_row_names, columns, df_version, hdf5_version);
}

}

}

#endif
