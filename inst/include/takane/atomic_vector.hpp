#ifndef TAKANE_ATOMIC_VECTOR_HPP
#define TAKANE_ATOMIC_VECTOR_HPP

#include "comservatory/comservatory.hpp"

#include "utils_csv.hpp"

#include <stdexcept>

/**
 * @file atomic_vector.hpp
 * @brief Validation for atomic vectors.
 */

namespace takane {

/**
 * @namespace takane::atomic_vector
 * @brief Definitions for atomic vectors.
 */
namespace atomic_vector {

/**
 * Type of the atomic vector.
 *
 * - `INTEGER`: a number that can be represented by a 32-bit signed integer.
 * - `NUMBER`: a number that can be represented by a double-precision float.
 * - `STRING`: a string.
 * - `BOOLEAN`: a boolean.
 */
enum class Type {
    INTEGER,
    NUMBER,
    STRING,
    BOOLEAN
};

/**
 * @brief Parameters for validating the atomic vector file.
 */
struct Parameters {
    /**
     * Length of the atomic vector.
     */
    size_t length = 0;

    /** 
     * Type of the atomic vector.
     */
    Type type = Type::INTEGER;

    /**
     * Whether the vector is named.
     */
    bool has_names = false;

    /**
     * Whether to load and parse the file in parallel, see `comservatory::ReadOptions` for details.
     */
    bool parallel = false;

    /**
     * Version of the `atomic_vector` format.
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
    if (params.has_names) {
        auto ptr = creator->string();
        output.fields.emplace_back(ptr);
        contents.fields.emplace_back(new CsvNameField(false, ptr));
    }

    switch(params.type) {
        case Type::INTEGER:
            {
                auto ptr = creator->integer();
                output.fields.emplace_back(ptr);
                contents.fields.emplace_back(new CsvIntegerField(static_cast<int>(params.has_names), ptr));
            }
            break;
        case Type::NUMBER:
            {
                auto ptr = creator->number();
                output.fields.emplace_back(nullptr);
                contents.fields.emplace_back(ptr);
            }
            break;
        case Type::STRING:
            {
                auto ptr = creator->string();
                output.fields.emplace_back(nullptr);
                contents.fields.emplace_back(ptr);
            }
            break;
        case Type::BOOLEAN:
            {
                auto ptr = creator->boolean();
                output.fields.emplace_back(nullptr);
                contents.fields.emplace_back(ptr);
            }
            break;
    }

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
 * Checks if a CSV is correctly formatted for the `atomic_vector` format.
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
 * Overload of `atomic_vector::validate()` that takes a file path.
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
