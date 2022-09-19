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
    void check_new_line(Contents& info) {
        if (on_header) {
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

            on_header = false;
        } else if (field_counter + 1 != info.fields.size()) {
            throw std::runtime_error("fewer fields than expected from the header");
        }
    }

    void add_entry(Contents& info, const char* x, size_t n) {
        if (buffer.size()) {
            buffer.insert(buffer.end(), x, x + n);
            add_entry_raw(info, buffer.data(), buffer.size());
            buffer.clear();
        } else {
            add_entry_raw(info, x, n);
        }
    }

    template<typename Char>
    void add_entry_raw(Contents& info, const Char* x, size_t n) {
        if (on_header) {
            info.names.push_back(to_string(x, n));
        } else {
            auto& everything = info.fields;
            if (field_counter >= everything.size()) {
                throw std::runtime_error("more fields than expected from the header");
            }

            auto* current = everything[field_counter].get();
            auto observed = decide_type(x, n);
            if (observed == UNKNOWN) {
                current->add_missing();
            } else {
                auto expected = current->type();
                if (expected == UNKNOWN) {
                    expected = observed;

                    bool use_dummy = check_store && 
                        to_store_by_name.find(info.names[field_counter]) == to_store_by_name.end() &&
                        to_store_by_index.find(field_counter) == to_store_by_index.end();

                    auto ptr = creator->create(observed, current->size(), use_dummy);
                    everything[field_counter].reset(ptr);

                    current = everything[field_counter].get();

                } else if (expected != observed) {
                    throw std::runtime_error("previous and current types do not match up");
                }

                switch (expected) {
                    case STRING:
                        static_cast<StringField*>(current)->push_back(to_string(x, n));
                        break;
                    case NUMBER:
                        static_cast<NumberField*>(current)->push_back(to_number(x, n));
                        break;
                    case COMPLEX:
                        static_cast<ComplexField*>(current)->push_back(to_complex(x, n));
                        break; 
                    case BOOLEAN:
                        static_cast<BooleanField*>(current)->push_back(to_boolean(x, n));
                        break; 
                    case UNKNOWN:
                        throw std::runtime_error("cannot add UNKNOWN type");
                }
            }
        }
        return;
    }

private:
    void parse_loop(const char* ptr, size_t length, Contents& info) {
        size_t previous = 0;
        for (size_t i = 0; i < length; ++i) {
            char c = ptr[i];

            if (on_string) {
                if (c == '"') {
                    on_quote = !on_quote;

                } else if (c == ',') {
                    if (on_quote) {
                        add_entry(info, ptr + previous, i - previous);
                        previous = i + 1; // skip past the comma.
                        ++field_counter;
                        on_string = false;
                        on_quote = false;
                    }

                } else if (c == '\n') {
                    if (on_quote) {
                        add_entry(info, ptr + previous, i - previous); 
                        check_new_line(info);
                        previous = i + 1; // skip past the newline.
                        field_counter = 0;
                        on_string = false;
                        on_quote = false;
                    }
                }
            } else {
                if (c == ',') {
                    add_entry(info, ptr + previous, i - previous);
                    previous = i + 1; // skip past the comma.
                    ++field_counter;

                } else if (c == '"') {
                    if (previous != i) {
                        throw std::runtime_error("encountered quote in the middle of a non-string entry");
                    }
                    on_string = true;

                } else if (c == '\n') {
                    if (field_counter == 0 && i == previous) {
                        // It's a zero-column file. If on_header is true, then
                        // this declares that there are no columns; if false but info.names 
                        // is empty, we're simply proceeding by not adding any values. 
                        if (on_header) {
                            on_header = false; 
                        } else if (info.names.empty()) {
                            ++info.fallback;
                        } else {
                            throw std::runtime_error("encountered empty line in a file with non-zero columns"); 
                        }
                    } else {
                        add_entry(info, ptr + previous, i - previous);
                        check_new_line(info);
                    }
                    previous = i + 1; // skip past the newline.
                    field_counter = 0;
                }
            }
        }

        // Storing content in the buffer.
        if (previous < length) {
            buffer.insert(buffer.end(), ptr + previous, ptr + length);
        }
    }

public:
    template<class Reader>
    void parse(Reader& reader, Contents& info, bool parallel) {
        if (parallel) {
            std::vector<unsigned char> holding;
            bool ok = reader();
            std::thread runner;

            while (1) {
                // Need to reinterpret_cast to interpret bytes as text.
                auto ptr = reinterpret_cast<const char*>(reader.buffer());
                size_t n = reader.available();

                if (!ok) {
                    parse_loop(ptr, n, info);
                    break;
                } 

                holding.resize(n);
                std::copy(ptr, ptr + n, holding.begin());
                runner = std::thread([&]() -> void { 
                    ok = reader(); 
                });

                try {
                    parse_loop(reinterpret_cast<const char*>(holding.data()), n, info);
                } catch (std::exception& e) {
                    runner.join();
                    throw;
                }

                runner.join();
            }

        } else {
            bool ok = true;
            while (ok) {
                ok = reader();
                size_t n = reader.available();
                auto ptr = reinterpret_cast<const char*>(reader.buffer());
                parse_loop(ptr, n, info);
            }
        }
    }

    void finish() {
        // If we didn't end on a newline, then field_counter > 0 (if more fields
        // were added) or the buffer is non-empty (if we're caught in the middle
        // of adding a new first field). 
        if (field_counter != 0 || buffer.size()) {
            if (on_string && !on_quote) {
                throw std::runtime_error("string not terminated by a double quote");
            } else {
                throw std::runtime_error("last record must be terminated by a single newline");
            }
        }
    }

private:
    bool on_header = true; 
    bool on_string = false;
    bool on_quote = false;
    std::vector<char> buffer;

    int field_counter = 0;
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
