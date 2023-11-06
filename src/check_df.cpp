#include "Rcpp.h"
#include "takane/csv_data_frame.hpp"
#include "takane/hdf5_data_frame.hpp"
#include "byteme/GzipFileReader.hpp"

static std::vector<takane::data_frame::ColumnDetails> configure_columns(
    const Rcpp::CharacterVector& column_names,
    const Rcpp::CharacterVector& column_types,
    const Rcpp::CharacterVector& string_formats,
    const Rcpp::LogicalVector& factor_ordered,
    const Rcpp::List& factor_levels) 
{
    size_t ncols = column_names.size();
    if (ncols != column_types.size()) {
        throw std::runtime_error("'column_names' and 'column_types' should have the same length");
    }
    if (ncols != string_formats.size()) {
        throw std::runtime_error("'column_names' and 'string_formats' should have the same length");
    }
    if (ncols != factor_ordered.size()) {
        throw std::runtime_error("'column_names' and 'factor_ordered' should have the same length");
    }
    if (ncols != factor_levels.size()) {
        throw std::runtime_error("'column_names' and 'factor_levels' should have the same length");
    }

    std::vector<takane::data_frame::ColumnDetails> columns(ncols);
    for (size_t c = 0; c < ncols; ++c) {
        auto& curcol = columns[c];
        curcol.name = Rcpp::as<std::string>(column_names[c]);

        auto curtype = Rcpp::as<std::string>(column_types[c]);
        if (curtype == "integer") {
            curcol.type = takane::data_frame::ColumnType::INTEGER;

        } else if (curtype == "number") {
            curcol.type = takane::data_frame::ColumnType::NUMBER;

        } else if (curtype == "string") {
            curcol.type = takane::data_frame::ColumnType::STRING;
            auto curformat = Rcpp::as<std::string>(string_formats[c]);
            if (curformat == "date") {
                curcol.string_format = takane::data_frame::StringFormat::DATE;
            } else if (curformat == "date-time") {
                curcol.string_format = takane::data_frame::StringFormat::DATE_TIME;
            }

        } else if (curtype == "date") {
            curcol.type = takane::data_frame::ColumnType::STRING;
            curcol.string_format = takane::data_frame::StringFormat::DATE;

        } else if (curtype == "date-time") {
            curcol.type = takane::data_frame::ColumnType::STRING;
            curcol.string_format = takane::data_frame::StringFormat::DATE_TIME;

        } else if (curtype == "boolean") {
            curcol.type = takane::data_frame::ColumnType::BOOLEAN;

        } else if (curtype == "factor" || curtype == "ordered") {
            curcol.type = takane::data_frame::ColumnType::FACTOR;
            curcol.factor_ordered = factor_ordered[c] || curtype == "ordered";
            Rcpp::CharacterVector levels(factor_levels[c]);
            auto& flevels = curcol.factor_levels.mutable_ref();
            for (size_t l = 0, end = levels.size(); l < end; ++l) {
                flevels.insert(Rcpp::as<std::string>(levels[l]));
            }

        } else if (curtype == "other") {
            curcol.type = takane::data_frame::ColumnType::OTHER;

        } else {
            throw std::runtime_error("unknown type code '" + curtype + "'");
        }
    }

    return columns;
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject check_csv_df(
    std::string path, 
    int nrows,
    bool has_row_names,
    Rcpp::CharacterVector column_names,
    Rcpp::CharacterVector column_types,
    Rcpp::CharacterVector string_formats,
    Rcpp::LogicalVector factor_ordered,
    Rcpp::List factor_levels,
    int df_version,
    bool is_compressed, 
    bool parallel) 
{
    takane::csv_data_frame::Parameters params;
    params.num_rows = nrows;
    params.has_row_names = has_row_names;
    params.columns = configure_columns(column_names, column_types, string_formats, factor_ordered, factor_levels);
    params.df_version = df_version;
    params.parallel = parallel;

    if (is_compressed) {
        byteme::GzipFileReader reader(path);
        takane::csv_data_frame::validate(reader, params);
    } else {
        byteme::RawFileReader reader(path);
        takane::csv_data_frame::validate(reader, params);
    }

    return R_NilValue;
}

//[[Rcpp::export(rng=false)]]
Rcpp::RObject check_hdf5_df(
    std::string path, 
    std::string name, 
    int nrows,
    bool has_row_names,
    Rcpp::CharacterVector column_names,
    Rcpp::CharacterVector column_types,
    Rcpp::CharacterVector string_formats,
    Rcpp::LogicalVector factor_ordered,
    Rcpp::List factor_levels,
    int df_version,
    int hdf5_version)
{
    takane::hdf5_data_frame::Parameters params(std::move(name));
    params.num_rows = nrows;
    params.has_row_names = has_row_names;
    params.columns = configure_columns(column_names, column_types, string_formats, factor_ordered, factor_levels);
    params.df_version = df_version;
    params.hdf5_version = hdf5_version;

    takane::hdf5_data_frame::validate(path.c_str(), params);
    return R_NilValue;
}
