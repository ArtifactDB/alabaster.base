#ifndef TAKANE_DATA_FRAME_HPP
#define TAKANE_DATA_FRAME_HPP

#include <unordered_set>
#include <string>

#include "WrappedOption.hpp"

/**
 * @file data_frame.hpp
 * @brief Common data frame utilities.
 */

namespace takane {

/**
 * @namespace takane::data_frame
 * @brief Definitions for abstract data frames.
 */
namespace data_frame {

/**
 * Type of the data frame column.
 *
 * - `INTEGER`: a number that can be represented by a 32-bit signed integer.
 * - `NUMBER`: a number that can be represented by a double-precision float.
 * - `STRING`: a string.
 * - `BOOLEAN`: a boolean.
 * - `FACTOR`: a categorical factor, typically represented as integer indices into an array of unique levels.
 * - `OTHER`: other column types that do not fall into the above categories.
 */
enum class ColumnType {
    INTEGER,
    NUMBER,
    STRING,
    BOOLEAN,
    FACTOR,
    OTHER
};

/**
 * Format of string columns in a data frame.
 *
 * - `NONE`: no format.
 * - `DATE`: date in the YYYY-MM-DD format.
 * - `DATE_TIME`: Internet date/time, following RFC3339.
 */
enum class StringFormat {
    NONE,
    DATE,
    DATE_TIME
};

/**
 * @brief Details for a column of a data frame.
 */
struct ColumnDetails {
    /**
     * Name of the column.
     */
    std::string name;

    /**
     * Type of the column.
     */
    ColumnType type = ColumnType::INTEGER;

    /**
     * Format of string columns, only used if `type == ColumnType::STRING`.
     */
    StringFormat format = StringFormat::NONE;

    /**
     * Unique factor levels, only used if `type == ColumnType::FACTOR`.
     */
    WrappedOption<std::unordered_set<std::string> > factor_levels;

    /**
     * Add a unique level to `factor_levels`, throwing an error if it already exists.
     * @param x Factor level to be added.
     */
    void add_factor_level(std::string x) {
        auto& levels = factor_levels.mutable_ref();
        if (levels.find(x) != levels.end()) {
            throw std::runtime_error("factor level '" + x + "' already exists in this column");
        }
        levels.insert(std::move(x));
    }
};

}

}

#endif
