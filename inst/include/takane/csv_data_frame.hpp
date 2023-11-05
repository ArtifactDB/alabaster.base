#ifndef TAKANE_CSV_DATA_FRAME_HPP
#define TAKANE_CSV_DATA_FRAME_HPP

#include "comservatory/comservatory.hpp"

#include "WrappedOption.hpp"
#include "data_frame.hpp"
#include "utils_csv.hpp"

#include <unordered_set>
#include <string>
#include <stdexcept>

/**
 * @file csv_data_frame.hpp
 * @brief Validation for CSV data frames.
 */

namespace takane {

/**
 * @namespace takane::csv_data_frame
 * @brief Definitions for CSV data frames.
 */
namespace csv_data_frame {

/**
 * @brief Parameters for validating the CSV data frame.
 */
struct Parameters {
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
     */
    WrappedOption<std::vector<data_frame::ColumnDetails> > columns;
 
    /**
     * Whether to load and parse the file in parallel, see `comservatory::ReadOptions` for details.
     */
    bool parallel = false;

    /**
     * Version of the `data_frame` format.
     */
    int df_version = 2;
};

/**
 * @cond
 */
template<class ParseCommand>
CsvContents validate_base(ParseCommand parse, const Parameters& params, CsvFieldCreator* creator) {
    DummyCsvFieldCreator default_creator;
    if (creator == NULL) {
        creator = &default_creator;
    }

    comservatory::Contents contents;
    CsvContents output;
    if (params.has_row_names) {
        auto ptr = creator->string();
        output.fields.emplace_back(ptr); 
        contents.fields.emplace_back(new CsvNameField(true, ptr));
    }

    const auto& columns = *(params.columns);
    size_t ncol = columns.size();
    std::unordered_set<std::string> present;

    for (size_t c = 0; c < ncol; ++c) {
        const auto& col = columns[c];
        if (present.find(col.name) != present.end()) {
            throw std::runtime_error("duplicate column name '" + col.name + "'");
        }
        present.insert(col.name);

        if (col.type == data_frame::ColumnType::INTEGER) {
            auto ptr = creator->integer();
            output.fields.emplace_back(ptr); 
            contents.fields.emplace_back(new CsvIntegerField(c, ptr));

        } else if (col.type == data_frame::ColumnType::NUMBER) {
            output.fields.emplace_back(nullptr); 
            contents.fields.emplace_back(creator->number());

        } else if (col.type == data_frame::ColumnType::STRING) {
            if (col.string_format == data_frame::StringFormat::DATE) {
                auto ptr = creator->string();
                output.fields.emplace_back(ptr); 
                contents.fields.emplace_back(new CsvDateField(c, ptr));

            } else if (col.string_format == data_frame::StringFormat::DATE_TIME) {
                auto ptr = creator->string();
                output.fields.emplace_back(ptr); 
                contents.fields.emplace_back(new CsvDateTimeField(c, ptr));

            } else {
                output.fields.emplace_back(nullptr);
                contents.fields.emplace_back(creator->string());
            }

        } else if (col.type == data_frame::ColumnType::BOOLEAN) {
            output.fields.emplace_back(nullptr);
            contents.fields.emplace_back(creator->boolean());

        } else if (col.type == data_frame::ColumnType::FACTOR) {
            if (params.df_version == 1) {
                auto ptr = creator->string();
                output.fields.emplace_back(ptr); 
                contents.fields.emplace_back(new CsvFactorV1Field(c, col.factor_levels.get(), ptr));
            } else {
                auto ptr = creator->integer();
                output.fields.emplace_back(ptr);
                contents.fields.emplace_back(new CsvFactorV2Field(c, col.factor_levels->size(), ptr));
            }

        } else if (col.type == data_frame::ColumnType::OTHER) {
            output.fields.emplace_back(nullptr);
            contents.fields.emplace_back(new comservatory::UnknownField); // This can be anything.

        } else {
            throw std::runtime_error("unknown code for the expected column type");
        }
    }

    comservatory::ReadOptions opt;
    opt.parallel = params.parallel;
    parse(contents, opt);
    if (contents.num_records() != params.num_rows) {
        throw std::runtime_error("number of records in the CSV file does not match the expected number of rows");
    }

    for (size_t c = 0; c < ncol; ++c) {
        const auto& col = columns[c];
        if (col.name != contents.names[c + params.has_row_names]) {
            throw std::runtime_error("observed and expected header names do not match");
        }
    }

    output.reconstitute(contents.fields);
    return output;
}
/**
 * @endcond
 */

/**
 * Checks if a CSV data frame is correctly formatted.
 * An error is raised if the file does not meet the specifications.
 *
 * @tparam Reader A **byteme** reader class.
 *
 * @param reader A stream of bytes from the CSV file.
 * @param params Validation parameters.
 * @param creator Factory to create objects for holding the contents of each CSV field.
 * Defaults to a pointer to a `DummyFieldCreator` instance.
 * 
 * @return Contents of the loaded CSV.
 * Whether the `fields` member actually contains the CSV data depends on `creator`.
 * Each entry of the `fields` member corresponds to an entry of `params.columns`,
 * which an additional field at the start if `params.has_row_names = true`.
 */
template<class Reader>
CsvContents validate(Reader& reader, const Parameters& params, CsvFieldCreator* creator = NULL) {
    return validate_base(
        [&](comservatory::Contents& contents, const comservatory::ReadOptions& opt) -> void { comservatory::read(reader, contents, opt); },
        params,
        creator
    );
}

/**
 * Overload of `csv_data_frame::validate()` that accepts a file path.
 *
 * @param path Path to the CSV file.
 * @param params Validation parameters.
 * @param creator Factory to create objects for holding the contents of each CSV field.
 * Defaults to a pointer to a `DummyFieldCreator` instance.
 * 
 * @return Contents of the loaded CSV.
 */
inline CsvContents validate(const char* path, const Parameters& params, CsvFieldCreator* creator = NULL) {
    return validate_base(
        [&](comservatory::Contents& contents, const comservatory::ReadOptions& opt) -> void { comservatory::read_file(path, contents, opt); },
        params,
        creator
    );
}

}

}

#endif
