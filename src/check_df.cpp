#include "Rcpp.h"
#include "takane/csv_data_frame.hpp"
#include "takane/hdf5_data_frame.hpp"
#include "byteme/GzipFileReader.hpp"

static std::vector<takane::data_frame::ColumnDetails> configure_columns(
    Rcpp::CharacterVector column_names,
    Rcpp::IntegerVector column_types,
    Rcpp::IntegerVector string_formats,
    Rcpp::List factor_levels) 
{
    size_t ncols = column_names.size();
    if (ncols != column_types.size()) {
        throw std::runtime_error("'column_names' and 'column_types' should have the same length");
    }
    if (ncols != string_formats.size()) {
        throw std::runtime_error("'column_names' and 'string_formats' should have the same length");
    }
    if (ncols != factor_levels.size()) {
        throw std::runtime_error("'column_names' and 'factor_levels' should have the same length");
    }

    std::vector<takane::data_frame::ColumnDetails> columns(ncols);
    for (size_t c = 0; c < ncols; ++c) {
        auto& curcol = columns[c];
        curcol.name = Rcpp::as<std::string>(column_names[c]);

        auto curtype = column_types[c];
        if (curtype == 0) {
            curcol.type = takane::data_frame::ColumnType::INTEGER;

        } else if (curtype == 1) {
            curcol.type = takane::data_frame::ColumnType::NUMBER;

        } else if (curtype == 2) {
            curcol.type = takane::data_frame::ColumnType::STRING;
            auto curformat = string_formats[c];
            if (curformat == 1) {
                curcol.format = takane::data_frame::StringFormat::DATE;
            } else if (curformat == 2) {
                curcol.format = takane::data_frame::StringFormat::DATE_TIME;
            }

        } else if (curtype == 3) {
            curcol.type = takane::data_frame::ColumnType::BOOLEAN;

        } else if (curtype == 4) {
            curcol.type = takane::data_frame::ColumnType::FACTOR;
            Rcpp::CharacterVector levels(factor_levels[c]);
            for (size_t l = 0, end = levels.size(); l < end; ++l) {
                curcol.add_factor_level(Rcpp::as<std::string>(levels[l]));
            }

        } else if (curtype == 5) {
            curcol.type = takane::data_frame::ColumnType::OTHER;

        } else {
            throw std::runtime_error("unknown type code '" + std::to_string(curtype) + "'");
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
    Rcpp::IntegerVector column_types,
    Rcpp::IntegerVector string_formats,
    Rcpp::List factor_levels,
    int df_version,
    bool is_compressed, 
    bool parallel) 
{
    takane::csv_data_frame::Parameters params;
    params.num_rows = nrows;
    params.has_row_names = has_row_names;
    params.columns = configure_columns(column_names, column_types, string_formats, factor_levels);
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
    Rcpp::IntegerVector column_types,
    Rcpp::IntegerVector string_formats,
    Rcpp::List factor_levels,
    int df_version,
    int hdf5_version)
{
    takane::hdf5_data_frame::Parameters params(std::move(name));
    params.num_rows = nrows;
    params.has_row_names = has_row_names;
    params.columns = configure_columns(column_names, column_types, string_formats, factor_levels);
    params.df_version = df_version;
    params.hdf5_version = hdf5_version;

    takane::hdf5_data_frame::validate(path.c_str(), params);
    return R_NilValue;
}
