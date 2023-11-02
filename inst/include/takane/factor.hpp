#ifndef TAKANE_FACTOR_HPP
#define TAKANE_FACTOR_HPP

#include "comservatory/comservatory.hpp"

#include "utils_csv.hpp"

#include <stdexcept>

/**
 * @file factor.hpp
 * @brief Validation for factors.
 */

namespace takane {

/**
 * @namespace takane::factor
 * @brief Definitions for abstract factors.
 */
namespace factor {

/**
 * @brief Parameters for validating the factor file.
 */
struct Parameters {
    /**
     * Length of the factor.
     */
    size_t length = 0;

    /**
     * Number of levels.
     */
    size_t num_levels = 0;

    /**
     * Whether the factor has names.
     */
    bool has_names = false;

    /**
     * Whether to load and parse the file in parallel, see `comservatory::ReadOptions` for details.
     */
    bool parallel = false;

    /**
     * Version of the `factor` format.
     */
    int version = 1;
};

/**
 * @cond
 */
template<class ParseCommand>
CsvContents validate_base(ParseCommand parse, const Parameters& params, CsvFieldCreator* creator = NULL) {
    DummyCsvFieldCreator default_creator;
    if (creator == NULL) {
        creator = &default_creator;
    }

    comservatory::Contents contents;
    CsvContents output;
    if (params.has_names) {
        auto ptr = creator->string();
        output.fields.emplace_back(ptr);
        contents.fields.emplace_back(new CsvNameField(false, ptr));
    }

    auto ptr = creator->integer();
    output.fields.emplace_back(ptr);
    contents.fields.emplace_back(new CsvFactorV2Field(static_cast<int>(params.has_names), params.num_levels, ptr)); 

    comservatory::ReadOptions opt;
    opt.parallel = params.parallel;
    parse(contents, opt);
    if (contents.num_records() != params.length) {
        throw std::runtime_error("number of records in the CSV file does not match the expected length");
    }

    if (contents.names.back() != "values") {
        throw std::runtime_error("column containing vector contents should be named 'values'");
    }

    output.reconstitute(contents.fields);
    return output;
}
/**
 * @endcond
 */

/**
 * Checks if a CSV is correctly formatted for the `factor` format.
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
 * If `params.has_names = true`, an additional column containing names is present at the start.
 */
template<class Reader>
CsvContents validate(Reader& reader, const Parameters& params, CsvFieldCreator* creator = NULL) {
    return validate_base(
        [&](comservatory::Contents& contents, const comservatory::ReadOptions& opts) -> void { comservatory::read(reader, contents, opts); },
        params,
        creator
    );
}

/**
 * Overload of `factor::validate()` that accepts a file path.
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
        [&](comservatory::Contents& contents, const comservatory::ReadOptions& opts) -> void { comservatory::read_file(path, contents, opts); },
        params,
        creator
    );
}

}

}

#endif
