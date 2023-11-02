#ifndef TAKANE_COMPRESSED_LIST_HPP
#define TAKANE_COMPRESSED_LIST_HPP

#include "comservatory/comservatory.hpp"

#include "utils_csv.hpp"

#include <stdexcept>

/**
 * @file compressed_list.hpp
 * @brief Validation for compressed lists.
 */

namespace takane {

/**
 * @namespace takane::compressed_list
 * @brief Definitions for compressed lists.
 */
namespace compressed_list {

/**
 * @brief Parameters for validating the compressed list file.
 */
struct Parameters {
    /**
     * Length of the compressed list.
     */
    size_t length = 0;
 
    /**
     * Total length of the concatenated elements.
     */
    size_t concatenated = 0;

    /**
     * Whether the compressed list is named.
     */
    bool has_names = false;

    /**
     * Whether to load and parse the file in parallel, see `comservatory::ReadOptions` for details.
     */
    bool parallel = false;

    /**
     * Version of the `compressed_list` format.
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

    auto ptr0 = creator->integer();
    output.fields.emplace_back(ptr0);
    auto ptr = new CsvCompressedLengthField(static_cast<int>(params.has_names), ptr0);
    contents.fields.emplace_back(ptr);

    comservatory::ReadOptions opt;
    opt.parallel = params.parallel;
    parse(contents, opt);
    if (contents.num_records() != params.length) {
        throw std::runtime_error("number of records in the CSV file does not match the expected length");
    }

    if (params.concatenated != ptr->total) {
        throw std::runtime_error("sum of lengths in the compressed list did not equal the expected concatenated total");
    }

    if (contents.names.back() != "number") {
        throw std::runtime_error("column containing the compressed list lengths should be named 'number'");
    }

    return output;
}
/**
 * @endcond
 */

/**
 * Checks if a CSV is correctly formatted for the `compressed_list` format.
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
 * If `params.has_names = true`, an additional field containing the names is present at the start.
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
 * Overload of `compressed_list::validate()` that accepts a file path.
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
