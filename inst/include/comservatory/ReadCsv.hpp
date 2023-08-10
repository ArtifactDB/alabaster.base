#ifndef COMSERVATORY_READ_CSV_HPP
#define COMSERVATORY_READ_CSV_HPP

#include <vector>
#include <string>

#include "Field.hpp"
#include "Parser.hpp"

#include "byteme/byteme.hpp"

/**
 * @file ReadCsv.hpp
 *
 * @brief Implements user-visible functions for loading a CSV file.
 */

namespace comservatory {

/**
 * @brief Read the contents of CSV files into memory.
 */
class ReadCsv {
public:
    /**
     * Whether to parallelize reading and parsing in multi-threaded environments.
     */
    bool parallel = false;

    /**
     * Whether to only validate the CSV structure, not store any of the data in memory.
     * If `true`, all settings of `creator`, `keep_subset`, `keep_subset_names` and `keep_subset_indices` are ignored.
     */
    bool validate_only = false;

    /**
     * Pointer to an instance of a concrete `FieldCreator` subclass.
     * If `NULL`, it defaults to an instance of an internal subclass that creates `FilledField` objects (or `DummyField`, if `dummy = true` in the `FieldCreator::create()` calls).
     */
    const FieldCreator* creator = nullptr; 

    /**
     * Whether to keep only a subset of fields.
     * If `false`, `keep_subset_names` and `keep_subset_indices` are ignored.
     */
    bool keep_subset = false;

    /** 
     * Container of strings with the names of the fields to retain.
     *
     * Field names do not have to actually be present in the CSV file - if absent, they are simply ignored.
     */
    std::vector<std::string> keep_subset_names;

    /**
     * Container of integers with the indices of the fields to retain.
     *
     * Indices are expected to non-negative and less than the total number of fields.
     * Values outside of this range will be ignored.
     */
    std::vector<int> keep_subset_indices;

private:
    template<class Reader>
    Contents load_internal(Reader& reader, const FieldCreator* creator) const {    
        Parser parser(creator);

        if (keep_subset) {
            parser.set_check_store(true);
            parser.set_store_by_name(keep_subset_names);
            parser.set_store_by_index(keep_subset_indices);
        }

        Contents output;
        parser.parse(reader, output, parallel);
        return output;
    }

public:
    /**
     * @tparam Reader A reader class that implements the same methods as `bytme::Reader`.
     *
     * @param reader Instance of a `Reader` class, containing the data stream for a CSV file.
     *
     * @return The `Contents` of the CSV file.
     *
     * If `keep_subset` is `true`, data is only loaded for the fields specified in `keep_subset_names` or `keep_subset_indices`;
     * all other fields are represented by dummy placeholders.
     *
     * If `validate_only` is `true`, all fields are represented by dummy placeholders.
     */
    template<class Reader>
    Contents read(Reader& reader) const {
        if (validate_only) {
            DefaultFieldCreator<true> creator;
            return load_internal(reader, &creator);
        } else if (creator) {
            return load_internal(reader, creator);
        } else {
            DefaultFieldCreator<false> creator;
            return load_internal(reader, &creator);
        }
    }

public:
    /**
     * @param path Path to a (possibly Gzipped) CSV file.
     *
     * @return The `Contents` of the CSV file.
     *
     * Gzip support requires linking to the Zlib library.
     */
    Contents read(const char* path) const {
#if __has_include("zlib.h")
        byteme::SomeFileReader reader(path);
#else
        byteme::RawFileReader reader(path);
#endif
        return read(reader);
    } 

    /**
     * @param path Path to a (possibly Gzipped) CSV file.
     *
     * @return The `Contents` of the CSV file.
     *
     * Gzip support requires linking to the Zlib library.
     */
    Contents read(std::string path) const {
        return read(path.c_str());
    } 
};

}

#endif
