#ifndef COMSERVATORY_READ_HPP
#define COMSERVATORY_READ_HPP

#include <vector>
#include <string>
#include "Creator.hpp"
#include "Parser.hpp"
#include "byteme/byteme.hpp"

/**
 * @file read.hpp
 *
 * @brief Read a CSV file.
 */

namespace comservatory {

/**
 * @brief Options for reading the contents of a CSV file.
 */
struct ReadOptions {
    /**
     * Whether to parallelize reading and parsing in multi-threaded environments.
     */
    bool parallel = false;

    /**
     * Whether to only validate the CSV structure, not store any of the data in memory.
     * If `true`, all fields in the output `Contents` are represented by dummy placeholders,
     * and all settings of `creator`, `keep_subset`, `keep_subset_names` and `keep_subset_indices` are ignored.
     */
    bool validate_only = false;

    /**
     * Pointer to an instance of a concrete `FieldCreator` subclass.
     * If `NULL`, it defaults to an instance of an internal subclass that creates `FilledField` objects (or `DummyField`, if `dummy = true` in the `FieldCreator::create()` calls).
     */
    const FieldCreator* creator = nullptr; 

    /**
     * Whether to keep only a subset of fields.
     * If `true`, data is only loaded for the fields specified in `keep_subset_names` or `keep_subset_indices`,
     * and all other fields are represented by dummy placeholders in the output `Contents`.
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
};

/**
 * @cond
 */
namespace internals {

inline Parser configure_parser(const FieldCreator* creator, const ReadOptions& options) {
    Parser parser(creator);
    if (options.keep_subset) {
        parser.set_check_store(true);
        parser.set_store_by_name(options.keep_subset_names);
        parser.set_store_by_index(options.keep_subset_indices);
    }
    return parser;
}

}
/**
 * @endcond
 */

/**
 * @tparam Reader A reader class that implements the same methods as `bytme::Reader`.
 *
 * @param reader Instance of a `Reader` class, containing the data stream for a CSV file.
 * @param contents `Contents` to store the parsed contents of the file.
 * @param options Reading options.
 *
 * `contents` can contain pre-filled `Contents::names`, which will be checked against the header names in the file.
 * Any differences in the observed header names and those in `Contents::names` will throw an error.
 *
 * `contents` can also contain pre-filled `Contents::fields`, which will be directly used for storing data from each column.
 * This is useful if the types of all columns are known in advance, and/or if certain columns need special handling via `Field` subclasses.
 * Any pre-filled field with an `UNKNOWN` type will be replaced via `Creator::create()`.
 */
template<class Reader>
void read(Reader& reader, Contents& contents, const ReadOptions& options) {
    if (options.validate_only) {
        DefaultFieldCreator<true> creator;
        auto parser = internals::configure_parser(&creator, options);
        parser.parse(reader, contents, options.parallel);
    } else if (options.creator) {
        auto parser = internals::configure_parser(options.creator, options);
        parser.parse(reader, contents, options.parallel);
    } else {
        DefaultFieldCreator<false> creator;
        auto parser = internals::configure_parser(&creator, options);
        parser.parse(reader, contents, options.parallel);
    }
}

/**
 * @tparam Reader A reader class that implements the same methods as `bytme::Reader`.
 *
 * @param reader Instance of a `Reader` class, containing the data stream for a CSV file.
 * @param options Reading options.
 *
 * @return The `Contents` of the CSV file.
 */
template<class Reader>
Contents read(Reader& reader, const ReadOptions& options) {
    Contents output;
    read(reader, output, options);
    return output;
}

/**
 * @param path Path to a (possibly Gzipped) CSV file.
 * @param contents `Contents` to store the parsed contents of the file, see the `read()` overload for details.
 * @param options Reading options.
 *
 * Gzip support requires linking to the Zlib library.
 */
inline void read_file(const char* path, Contents& contents, const ReadOptions& options) {
#if __has_include("zlib.h")
    byteme::SomeFileReader reader(path);
#else
    byteme::RawFileReader reader(path);
#endif
    read(reader, contents, options);
} 

/**
 * @param path Path to a (possibly Gzipped) CSV file.
 * @param options Reading options.
 *
 * @return The `Contents` of the CSV file.
 *
 * Gzip support requires linking to the Zlib library.
 */
inline Contents read_file(const char* path, const ReadOptions& options) {
    Contents output;
    read_file(path, output, options);
    return output;
} 

/**
 * @param path Path to a (possibly Gzipped) CSV file.
 *
 * @return The `Contents` of the CSV file.
 *
 * Gzip support requires linking to the Zlib library.
 */
inline Contents read_file(const char* path) {
    return read_file(path, ReadOptions());
} 

/**
 * @param path Path to a (possibly Gzipped) CSV file.
 * @param contents `Contents` to store the parsed contents of the file, see the `read()` overload for details.
 * @param options Reading options.
 *
 * @return The `Contents` of the CSV file.
 *
 * Gzip support requires linking to the Zlib library.
 */
inline void read_file(const std::string& path, Contents& contents, const ReadOptions& options) {
    read_file(path.c_str(), contents, options);
} 

/**
 * @param path Path to a (possibly Gzipped) CSV file.
 * @param options Reading options.
 *
 * @return The `Contents` of the CSV file.
 *
 * Gzip support requires linking to the Zlib library.
 */
inline Contents read_file(const std::string& path, const ReadOptions& options) {
    return read_file(path.c_str(), options);
} 

/**
 * @param path Path to a (possibly Gzipped) CSV file.
 *
 * @return The `Contents` of the CSV file.
 *
 * Gzip support requires linking to the Zlib library.
 */
inline Contents read_file(const std::string& path) {
    return read_file(path.c_str(), ReadOptions());
} 


}

#endif
