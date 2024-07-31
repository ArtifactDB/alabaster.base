#ifndef TAKANE_UTILS_PUBLIC_HPP
#define TAKANE_UTILS_PUBLIC_HPP

#include <string>
#include <filesystem>
#include <unordered_map>
#include <functional>

#include "H5Cpp.h"

#include "millijson/millijson.hpp"
#include "byteme/byteme.hpp"
#include "chihaya/chihaya.hpp"
#include "utils_json.hpp"

/**
 * @file utils_public.hpp
 * @brief Exported utilities.
 */

namespace takane {

/**
 * @brief Object metadata, including the type and other fields.
 */
struct ObjectMetadata {
    /**
     * Type of the object.
     */
    std::string type;
    
    /**
     * Other fields, depending on the object type.
     */
    std::unordered_map<std::string, std::shared_ptr<millijson::Base> > other;
};

/**
 * Parses a JSON object to obtain the object metadata.
 *
 * @param raw Raw JSON object, typically obtained by `millijson::parse_file`.
 * Note that `raw` is consumed by this function and should no longer be used by the caller.
 * @return Object metadata, including the type and other fields.
 */
inline ObjectMetadata reformat_object_metadata(millijson::Base* raw) {
    if (raw->type() != millijson::OBJECT) {
        throw std::runtime_error("metadata should be a JSON object");
    }

    ObjectMetadata output;
    output.other = std::move(reinterpret_cast<millijson::Object*>(raw)->values);

    auto tIt = output.other.find("type");
    if (tIt == output.other.end()) {
        throw std::runtime_error("metadata should have a 'type' property");
    }

    const auto& tval = tIt->second;
    if (tval->type() != millijson::STRING) {
        throw std::runtime_error("metadata should have a 'type' string");
    }

    output.type = std::move(reinterpret_cast<millijson::String*>(tval.get())->value);
    output.other.erase(tIt);
    return output;
}

/**
 * Reads the `OBJECT` file inside a directory to determine the object type.
 *
 * @param path Path to a directory containing an object.
 * @return Object metadata, including the type and other fields.
 */
inline ObjectMetadata read_object_metadata(const std::filesystem::path& path) try {
    std::shared_ptr<millijson::Base> obj = internal_json::parse_file(path / "OBJECT");
    return reformat_object_metadata(obj.get());
} catch (std::exception& e) {
    throw std::runtime_error("failed to read the OBJECT file at '" + path.string() + "'; " + std::string(e.what()));
}

/**
 * @brief Validation options.
 *
 * Collection of optional parameters to fine-tune the behavior of various **takane** functions.
 * This can be configured by applications to, e.g., add more validation functions for custom types or to increase the strictness of some checks.
 *
 * Most **takane** functions will accept a non-`const` reference to an `Options` object.
 * The lack of `const`-ness is intended to support custom functions that mutate some external variable, e.g., to collect statistics for certain object types.
 * While unusual, it is permissible for a **takane** function to modify the supplied `Options`, as long as that modification is reversed upon exiting the function.
 *
 * The possibility for modification means that calls to **takane** functions are effectively `const` but not thread-safe with respect to any single `Options` instance.
 * If thread safety is needed, it is best achieved by creating a separate `Options` instance for use in each thread.
 */
struct Options {
    /**
     * Whether to parallelize reading from disk and parsing, when available.
     */
    bool parallel_reads = true;
    
    /**
     * Buffer size to use when reading data from a HDF5 file.
     */
    hsize_t hdf5_buffer_size = 10000;

public:
    /**
     * Custom registry of functions to be used by `validate()`.
     * Each key is an object type and each value is a function that accepts the same arguments as `validate()`.
     * If a type is specified here, the custom function replaces the default.
     */
    std::unordered_map<std::string, std::function<void(const std::filesystem::path&, const ObjectMetadata&, Options&)> > custom_validate;

    /**
     * Addtional validation function to run for all object types during a call to `validate()`, after running the (default or custom) type-specific validation function.
     * Arguments for this function are as described for `validate()`.
     */
   std::function<void(const std::filesystem::path&, const ObjectMetadata&, Options&)> custom_global_validate;

public:
    /**
     * Custom registry of functions to be used by `dimensions()`.
     * Each key is an object type and each value is a function that accepts the same arguments as `dimensions()`.
     * If a type is specified here, the custom function replaces the default. 
     */
    std::unordered_map<std::string, std::function<std::vector<size_t>(const std::filesystem::path&, const ObjectMetadata&, Options&)> > custom_dimensions;

    /**
     * Custom registry of functions to be used by `height()`.
     * Each key is an object type and each value is a function that accepts the same arguments as `height()`.
     * If a type is specified here, the custom function replaces the default. 
     */
    std::unordered_map<std::string, std::function<size_t(const std::filesystem::path&, const ObjectMetadata& m, Options&)> > custom_height;

public:
    /**
     * Custom registry of derived object types and their base types, to be used by `derived_from()`.
     * Each key is the base object type and each value is the set of its derived types.
     * If a type is specified here, the set of derived types is added to the the default set.
     */
    std::unordered_map<std::string, std::unordered_set<std::string> > custom_derived_from;

    /**
     * Custom registry of object types that satisfy a particular object interface.
     * Each key is the interface and each value is the set of all types that satisfy it.
     * If a type is specified here, its set of types is added to the the default set.
     */
    std::unordered_map<std::string, std::unordered_set<std::string> > custom_satisfies_interface;

public:
    /**
     * Application-specific function to check the validity of a BAM file and its indices in `bam_file::validate()`.
     * This should accept a path to the directory containing the BAM file and indices, the object metadata, and additional reading options.
     * It should throw an error if the BAM file is not valid, e.g., corrupted file, mismatched indices.
     */
    std::function<void(const std::filesystem::path&, const ObjectMetadata&, Options&)> bam_file_strict_check;

    /**
     * Application-specific function to check the validity of a BCF file and its indices in `bcf_file::validate()`.
     * This should accept a path to the directory containing the BCF file and indices, the object metadata, and additional reading options.
     * It should throw an error if the BCF file is not valid, e.g., corrupted file, mismatched indices.
     */
    std::function<void(const std::filesystem::path&, const ObjectMetadata&, Options&)> bcf_file_strict_check;

    /**
     * Application-specific function to check the validity of a BED file and its indices in `bed_file::validate()`.
     * This should accept a path to the directory containing the BED file, the object metadata, additional reading options,
     * and a boolean indicating whether indices are expected to be present in the directory.
     * It should throw an error if the BED file is not valid, e.g., corrupted file, mismatched indices.
     */
    std::function<void(const std::filesystem::path&, const ObjectMetadata&, Options&, bool)> bed_file_strict_check;

    /**
     * Application-specific function to check the validity of a bigBed file in `bigbed_file::validate()`.
     * This should accept a path to the directory containing the bigBed file, the object metadata, and additional reading options.
     * It should throw an error if the bigBed file is not valid, e.g., corrupted file, mismatched indices.
     */
    std::function<void(const std::filesystem::path&, const ObjectMetadata&, Options&)> bigbed_file_strict_check;

    /**
     * Application-specific function to check the validity of a bigWig file in `bigwig_file::validate()`.
     * This should accept a path to the directory containing the bigWig file, the object metadata, and additional reading options.
     * It should throw an error if the bigWig file is not valid, e.g., corrupted file, mismatched indices.
     */
    std::function<void(const std::filesystem::path&, const ObjectMetadata&, Options&)> bigwig_file_strict_check;

    /**
     * Application-specific function to determine whether there are duplicated rows in the data frame containing the levels of a data frame factor, to be used in `data_frame_factor::validate()`
     * This should accept a path to the directory containing the data frame, the object metadata, and additional reading options.
     * It should return a boolean indicating whether any duplicate rows were found. 
     */
    std::function<bool(const std::filesystem::path&, const ObjectMetadata&, Options& options)> data_frame_factor_any_duplicated;

    /**
     * Application-specific function to check the validity of a FASTA file and its indices in `fasta_file::validate()`.
     * This should accept a path to the directory containing the FASTA file, the object metadata, additional reading options,
     * and a boolean indicating whether indices are expected to be present in the directory.
     * It should throw an error if the FASTA file is not valid, e.g., corrupted file, mismatched indices.
     */
    std::function<void(const std::filesystem::path&, const ObjectMetadata&, Options&, bool)> fasta_file_strict_check;

    /**
     * Application-specific function to check the validity of a FASTQ file and its indices in `fastq_file::validate()`.
     * This should accept a path to the directory containing the FASTQ file, the object metadata, additional reading options, 
     * and a boolean indicating whether or not indices are expected to be present in the directory.
     * It should throw an error if the FASTQ file is not valid, e.g., corrupted file, mismatched indices.
     */
    std::function<void(const std::filesystem::path&, const ObjectMetadata&, Options&, bool)> fastq_file_strict_check;

    /**
     * Application-specific function to check the validity of a GFF file and its indices in `gff_file::validate()`.
     * This should accept a path to the directory containing the GFF file, the object metadata, additional reading options.
     * and a boolean indicating whether indices are expected to be present in the directory.
     * It should throw an error if the GFF file is not valid, e.g., corrupted file, mismatched indices.
     */
    std::function<void(const std::filesystem::path&, const ObjectMetadata&, Options&, bool)> gff_file_strict_check;

    /**
     * Application-specific function to check the validity of a GMT file and its indices in `gmt_file::validate()`.
     * This should accept a path to the directory containing the GMT file, the object metadata and additional reading options.
     * It should throw an error if the GMT file is not valid, e.g., corrupted file, mismatched indices.
     */
    std::function<void(const std::filesystem::path&, const ObjectMetadata&, Options&)> gmt_file_strict_check;

    /**
     * Application-specific function to check the validity of a RDS file and its indices in `rds_file::validate()`.
     * This should accept a path to the directory containing the RDS file, the object metadata and additional reading options.
     * It should throw an error if the RDS file is not valid, e.g., corrupted file, mismatched indices.
     */
    std::function<void(const std::filesystem::path&, const ObjectMetadata&, Options&)> rds_file_strict_check;

    /**
     * Options to use for validating **chihaya** specifications in `delayed_array::validate()`.
     */
    chihaya::Options delayed_array_options;
};

}

#endif
