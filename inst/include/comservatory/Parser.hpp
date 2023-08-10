#ifndef COMSERVATORY_PARSER_HPP
#define COMSERVATORY_PARSER_HPP

#include <vector>
#include <string>
#include <stdexcept>
#include <unordered_set>
#include <algorithm>
#include <memory>
#include <thread>

#include "convert.hpp"
#include "Field.hpp"
#include "Creator.hpp"

#include "byteme/PerByte.hpp"

/**
 * @file Parser.hpp
 *
 * @brief Defines the `Contents` class for storing the CSV contents.
 */

namespace comservatory {

/**
 * @cond
 */
struct Parser;
/**
 * @endcond
 */

/**
 * @brief The parsed contents of a CSV file.
 */
struct Contents {
    /**
     * Vector of data for each of the fields in the CSV file.
     */
    std::vector<std::unique_ptr<Field> > fields;

    /**
     * Vector of names for the fields in the CSV file.
     * This is of length equal to `fields`.
     */
    std::vector<std::string> names;

    /**
     * @return Number of fields in the CSV file.
     */
    size_t num_fields () const { 
        return names.size(); 
    }

    /**
     * @return Number of records in the CSV file.
     */
    size_t num_records () const {
        return (num_fields() ? fields[0]->size() : fallback);
    }

private:
    friend Parser;
    size_t fallback = 0;
};

/**
 * @cond
 */
struct Parser {
public:
    Parser(const FieldCreator* f) : creator(f) {}

public:
    Parser& set_check_store(bool s = false) {
        check_store = s;
        return *this;
    }

    template<class NameIter>
    Parser& set_store_by_name(NameIter start, NameIter end) {
        to_store_by_name = std::unordered_set<std::string>(start, end);
        return *this;
    }

    template<class NameContainer>
    Parser& set_store_by_name(const NameContainer& k) {
        return set_store_by_name(k.begin(), k.end());
    }

    template<class IndexIter>
    Parser& set_store_by_index(IndexIter start, IndexIter end) {
        to_store_by_index = std::unordered_set<size_t>(start, end);
        return *this;
    }

    template<class IndexContainer>
    Parser& set_store_by_index(const IndexContainer& k) {
        return set_store_by_index(k.begin(), k.end());
    }

private:
    static Field* fetch_column(Contents& info, size_t column, size_t line) {
        auto& everything = info.fields;
        if (column >= everything.size()) {
            throw std::runtime_error("more fields on line " + std::to_string(line + 1) + " than expected from the header");
        }
        return everything[column].get();
    }

    Field* check_column_type(Contents& info, Type observed, size_t column, size_t line) const {
        Field* current = fetch_column(info, column, line);
        auto expected = current->type();

        if (expected == UNKNOWN) {
            bool use_dummy = check_store && 
                to_store_by_name.find(info.names[column]) == to_store_by_name.end() &&
                to_store_by_index.find(column) == to_store_by_index.end();

            auto ptr = creator->create(observed, current->size(), use_dummy);
            info.fields[column].reset(ptr);
            current = info.fields[column].get();
        } else if (expected != observed) {
            throw std::runtime_error("previous and current types do not match up");
        }

        return current;
    }

    template<class Input>
    void store_nan(Input& input, Contents& info, size_t column, size_t line) const {
        input.advance();
        expect_fixed(input, "an", "AN", column, line); // i.e., NaN or any of its capitalizations.
        auto* current = check_column_type(info, NUMBER, column, line);
        static_cast<NumberField*>(current)->push_back(std::numeric_limits<double>::quiet_NaN());
    }

    template<class Input>
    void store_inf(Input& input, Contents& info, size_t column, size_t line, bool negative) const {
        input.advance();
        expect_fixed(input, "nf", "NF", column, line); // i.e., Inf or any of its capitalizations.
        auto* current = check_column_type(info, NUMBER, column, line);

        double val = std::numeric_limits<double>::infinity();
        if (negative) {
            val *= -1;
        }
        static_cast<NumberField*>(current)->push_back(val);
    }

    template<class Input>
    void store_na_or_nan(Input& input, Contents& info, size_t column, size_t line) const {
        // Some shenanigans required here to distinguish between
        // NAN/NaN/etc. and NA, given that both are allowed.
        input.advance();
        if (!input.valid()) {
            throw std::runtime_error("truncated keyword in " + get_location(column, line));
        }

        char second = input.get();
        bool is_missing = true;
        if (second == 'a') {
            is_missing = false;
        } else if (second != 'A') {
            throw std::runtime_error("unknown keyword in " + get_location(column, line));
        }

        input.advance();
        if (!input.valid()) {
            if (is_missing) {
                throw std::runtime_error("line " + std::to_string(line + 1) + " should terminate with a newline");
            } else {
                throw std::runtime_error("truncated keyword in " + get_location(column, line));
            }
        }

        char next = input.get();
        if (next == 'n' || next == 'N') {
            auto* current = check_column_type(info, NUMBER, column, line);
            static_cast<NumberField*>(current)->push_back(std::numeric_limits<double>::quiet_NaN());
            input.advance(); // for consistency with the NA case, in the sense that we are always past the keyword regardless of whether the keyword is NaN or NA.
        } else if (is_missing) {
            auto raw = fetch_column(info, column, line);
            raw->add_missing();
        } else {
            throw std::runtime_error("unknown keyword in " + get_location(column, line));
        }
    }

    template<class Input>
    void store_number_or_complex(Input& input, Contents& info, size_t column, size_t line, bool negative) {
        auto first = to_number(input, column, line);
        if (negative) {
            first *= -1;
        }

        char next = input.get(); // no need to check validity, as to_number always leaves us on a valid position (or throws itself).
        if (next == ',' || next == '\n') {
            auto* current = check_column_type(info, NUMBER, column, line);
            static_cast<NumberField*>(current)->push_back(first);
            return;
        }

        char second_neg = false;
        if (next == '-') {
            second_neg = true;
        } else if (next != '+') {
            throw std::runtime_error("incorrectly formatted number in " + get_location(column, line));
        }

        input.advance();
        if (!input.valid()) {
            throw std::runtime_error("truncated complex number in " + get_location(column, line));
        } else if (!std::isdigit(input.get())) {
            throw std::runtime_error("incorrectly formatted complex number in " + get_location(column, line));
        }

        auto second = to_number(input, column, line);
        if (second_neg) {
            second *= -1;
        }
        if (input.get() != 'i') { // again, no need to check validity.
            throw std::runtime_error("incorrectly formatted complex number in " + get_location(column, line));
        }
        input.advance(); // for consistency with the numbers, in the sense that we are always past the keyword regardless of whether we're a NUMBER or COMPLEX.

        auto* current = check_column_type(info, COMPLEX, column, line);
        static_cast<ComplexField*>(current)->push_back(std::complex<double>(first, second));
    }

private:
    template<class Input>
    void parse_loop(Input& input, Contents& info) {
        if (!input.valid()) {
            throw std::runtime_error("CSV file is empty");
        }

        // Special case for a new-line only file.
        if (input.get() == '\n') {
            auto& line = info.fallback;
            while (1) {
                input.advance();
                if (!input.valid()) {
                    break;
                }
                ++line;
                if (input.get() != '\n') {
                    throw std::runtime_error("more fields on line " + std::to_string(line + 1) + " than expected from the header");
                }
            }
            return;
        }

        // Processing the header.
        while (1) {
            char c = input.get();
            if (c != '"') {
                throw std::runtime_error("all headers should be quoted strings");
            }

            info.names.push_back(to_string(input, info.names.size(), 0)); // no need to check validity, as to_string always leaves us on a valid position (or throws itself).

            char next = input.get();
            input.advance();
            if (next == '\n') {
                break;
            } else if (next != ',') {
                throw std::runtime_error("header " + std::to_string(info.names.size()) + " contains trailing character '" + std::string(1, next) + "'"); 
            }
        } 

        auto copy = info.names;
        std::sort(copy.begin(), copy.end());
        for (size_t s = 1; s < copy.size(); ++s) {
            if (copy[s] == copy[s-1]) {
                throw std::runtime_error("detected duplicated header names");
            }
        }

        info.fields.resize(info.names.size());
        for (auto& o : info.fields) {
            o.reset(new UnknownField);
        }

        // Special case if there are no records, i.e., it's header-only.
        if (!input.valid()) {
            return;
        }

        // Processing the records in a CSV.
        size_t column = 0;
        size_t line = 1;
        while (1) {
            switch (input.get()) {
                case '"':
                    {
                        auto* current = check_column_type(info, STRING, column, line);
                        static_cast<StringField*>(current)->push_back(to_string(input, column, line));
                    }
                    break;

                case 't': case 'T':
                    {
                        input.advance();
                        expect_fixed(input, "rue", "RUE", column, line);
                        auto* current = check_column_type(info, BOOLEAN, column, line);
                        static_cast<BooleanField*>(current)->push_back(true);
                    }
                    break;

                case 'f': case 'F':
                    {
                        input.advance();
                        expect_fixed(input, "alse", "ALSE", column, line);
                        auto* current = check_column_type(info, BOOLEAN, column, line);
                        static_cast<BooleanField*>(current)->push_back(false);
                    }
                    break;

                case 'N':
                    store_na_or_nan(input, info, column, line);
                    break;

                case 'n': 
                    store_nan(input, info, column, line);
                    break;
                
                case 'i': case 'I':
                    store_inf(input, info, column, line, false);
                    break;

                case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                    store_number_or_complex(input, info, column, line, false);
                    break;

                case '+':
                    input.advance();
                    if (!input.valid()) {
                        throw std::runtime_error("truncated field in " + get_location(column, line)); 
                    } else if (!std::isdigit(input.get())) {
                        throw std::runtime_error("invalid number in " + get_location(column, line)); 
                    }
                    store_number_or_complex(input, info, column, line, false);
                    break;

                case '-':
                    {
                        input.advance();
                        if (!input.valid()) {
                            throw std::runtime_error("truncated field in " + get_location(column, line));
                        }

                        char next = input.get();
                        if (next == 'i' || next == 'I') {
                            store_inf(input, info, column, line, true);
                        } else if (next == 'n' || next == 'N') {
                            store_nan(input, info, column, line);
                        } else if (std::isdigit(next)) {
                            store_number_or_complex(input, info, column, line, true);
                        } else {
                            throw std::runtime_error("incorrectly formatted number in " + get_location(column, line));
                        }
                    }
                    break;

                case '\n':
                    throw std::runtime_error(get_location(column, line) + " is empty");

                default:
                    throw std::runtime_error("unknown type starting with '" + std::string(1, input.get()) + "' in " + get_location(column, line));
            }

            if (!input.valid()) {
                throw std::runtime_error("last line must be terminated by a single newline");
            }

            char next = input.get();
            input.advance();
            if (next == ',') {
                ++column;
                if (!input.valid()) {
                    throw std::runtime_error("line " + std::to_string(line + 1) + " is truncated at column " + std::to_string(column + 1));
                }
            } else if (next == '\n') {
                if (column + 1 != info.names.size()) {
                    throw std::runtime_error("line " + std::to_string(line + 1) + " has fewer fields than expected from the header");
                }
                if (!input.valid()) {
                    break;
                }
                column = 0;
                ++line;
            } else {
                throw std::runtime_error(get_location(column, line) + " contains trailing character '" + std::string(1, next) + "'"); 
            }
        }
    }

public:
    template<class Reader>
    void parse(Reader& reader, Contents& info, bool parallel) {
        if (parallel) {
            byteme::PerByteParallel input(&reader);
            parse_loop(input, info);
        } else {
            byteme::PerByte input(&reader);
            parse_loop(input, info);
        }
    }

    const FieldCreator* creator;

    bool check_store = false;
    std::unordered_set<std::string> to_store_by_name;
    std::unordered_set<size_t> to_store_by_index;
};
/**
 * @endcond
 */

}

#endif
