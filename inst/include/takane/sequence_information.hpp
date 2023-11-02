#ifndef TAKANE_SEQUENCE_INFORMATION_HPP
#define TAKANE_SEQUENCE_INFORMATION_HPP

#include "comservatory/comservatory.hpp"

#include "data_frame.hpp"
#include "utils_csv.hpp"

#include <stdexcept>

/**
 * @file sequence_information.hpp
 * @brief Validation for sequence information.
 */

namespace takane {

/**
 * @namespace takane::sequence_information
 * @brief Definitions for sequence information objects.
 */
namespace sequence_information {

/**
 * @brief Parameters for validating the sequence information file.
 */
struct Parameters {
    /**
     * Expected number of sequences.
     */
    size_t num_sequences = 0;

    /**
     * Whether to load and parse the file in parallel, see `comservatory::ReadOptions` for details.
     */
    bool parallel = false;

    /**
     * Version of the `sequence_information` format.
     */
    int version = 1;
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
    contents.names.push_back("seqnames");
    {
        auto ptr = creator->string();
        output.fields.emplace_back(ptr);
        contents.fields.emplace_back(new CsvUniqueStringField(0, ptr));
    }

    contents.names.push_back("seqlengths");
    {
        auto ptr = creator->integer();
        output.fields.emplace_back(ptr);
        contents.fields.emplace_back(new CsvNonNegativeIntegerField(1, ptr));
    }

    contents.names.push_back("isCircular");
    output.fields.emplace_back(nullptr);
    contents.fields.emplace_back(creator->boolean());

    contents.names.push_back("genome");
    output.fields.emplace_back(nullptr);
    contents.fields.emplace_back(creator->string());

    comservatory::ReadOptions opt;
    opt.parallel = params.parallel;
    parse(contents, opt);
    if (contents.num_records() != params.num_sequences) {
        throw std::runtime_error("number of records in the CSV file does not match the expected number of ranges");
    }

    output.reconstitute(contents.fields);
    return output;
}
/**
 * @endcond
 */

/**
 * Checks if a CSV data frame is correctly formatted for sequence information.
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
 * Overload of `sequence_information::validate()` that accepts a file path.
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
