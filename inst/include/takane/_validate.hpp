#ifndef TAKANE_VALIDATE_HPP
#define TAKANE_VALIDATE_HPP

#include <functional>
#include <string>
#include <stdexcept>
#include <filesystem>

#include "utils_public.hpp"
#include "atomic_vector.hpp"
#include "string_factor.hpp"
#include "simple_list.hpp"
#include "data_frame.hpp"
#include "data_frame_factor.hpp"
#include "sequence_information.hpp"
#include "genomic_ranges.hpp"
#include "atomic_vector_list.hpp"
#include "data_frame_list.hpp"
#include "genomic_ranges_list.hpp"
#include "dense_array.hpp"
#include "compressed_sparse_matrix.hpp"
#include "summarized_experiment.hpp"
#include "ranged_summarized_experiment.hpp"
#include "single_cell_experiment.hpp"
#include "spatial_experiment.hpp"
#include "multi_sample_dataset.hpp"
#include "sequence_string_set.hpp"
#include "bam_file.hpp"
#include "bcf_file.hpp"
#include "bigwig_file.hpp"
#include "bigbed_file.hpp"
#include "fasta_file.hpp"
#include "fastq_file.hpp"
#include "bed_file.hpp"
#include "gmt_file.hpp"
#include "gff_file.hpp"
#include "bumpy_atomic_array.hpp"
#include "bumpy_data_frame_array.hpp"
#include "vcf_experiment.hpp"
#include "delayed_array.hpp"

/**
 * @file _validate.hpp
 * @brief Validation dispatch function.
 */

namespace takane {

/**
 * Class to map object types to `validate()` functions.
 */
typedef std::unordered_map<std::string, std::function<void(const std::filesystem::path&, const ObjectMetadata&, const Options&)> > ValidateRegistry;

/**
 * @cond
 */
namespace internal_validate {

inline ValidateRegistry default_registry() {
    ValidateRegistry registry;
    registry["atomic_vector"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { atomic_vector::validate(p, m, o); };
    registry["string_factor"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { string_factor::validate(p, m, o); };
    registry["simple_list"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { simple_list::validate(p, m, o); };
    registry["data_frame"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { data_frame::validate(p, m, o); };
    registry["data_frame_factor"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { data_frame_factor::validate(p, m, o); };
    registry["sequence_information"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { sequence_information::validate(p, m, o); };
    registry["genomic_ranges"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { genomic_ranges::validate(p, m, o); };
    registry["atomic_vector_list"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { atomic_vector_list::validate(p, m, o); };
    registry["data_frame_list"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { data_frame_list::validate(p, m, o); };
    registry["genomic_ranges_list"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { genomic_ranges_list::validate(p, m, o); };
    registry["dense_array"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { dense_array::validate(p, m, o); };
    registry["compressed_sparse_matrix"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { compressed_sparse_matrix::validate(p, m, o); };
    registry["summarized_experiment"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { summarized_experiment::validate(p, m, o); };
    registry["ranged_summarized_experiment"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { ranged_summarized_experiment::validate(p, m, o); };
    registry["single_cell_experiment"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { single_cell_experiment::validate(p, m, o); };
    registry["spatial_experiment"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { spatial_experiment::validate(p, m, o); };
    registry["multi_sample_dataset"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { multi_sample_dataset::validate(p, m, o); };
    registry["sequence_string_set"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { sequence_string_set::validate(p, m, o); };
    registry["bam_file"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { bam_file::validate(p, m, o); };
    registry["bcf_file"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { bcf_file::validate(p, m, o); };
    registry["bigwig_file"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { bigwig_file::validate(p, m, o); };
    registry["bigbed_file"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { bigbed_file::validate(p, m, o); };
    registry["fasta_file"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { fasta_file::validate(p, m, o); };
    registry["fastq_file"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { fastq_file::validate(p, m, o); };
    registry["bed_file"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { bed_file::validate(p, m, o); };
    registry["gmt_file"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { gmt_file::validate(p, m, o); };
    registry["gff_file"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { gff_file::validate(p, m, o); };
    registry["bumpy_atomic_array"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { bumpy_atomic_array::validate(p, m, o); };
    registry["bumpy_data_frame_array"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { bumpy_data_frame_array::validate(p, m, o); };
    registry["vcf_experiment"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { vcf_experiment::validate(p, m, o); };
    registry["delayed_array"] = [](const std::filesystem::path& p, const ObjectMetadata& m, const Options& o) { delayed_array::validate(p, m, o); };
    return registry;
} 

}
/**
 * @endcond
 */

/**
 * Registry of functions to be used by `validate()`.
 * Applications can extend **takane** by adding new validation functions for custom object types.
 */
inline ValidateRegistry validate_registry = internal_validate::default_registry();

/**
 * Validate an object in a subdirectory, based on the supplied object type.
 * This searches the `validate_registry` to find a validation function for the given type.
 *
 * @param path Path to a directory representing an object.
 * @param metadata Metadata for the object, typically determined from its `OBJECT` file.
 * @param options Validation options, mostly for input performance.
 */
inline void validate(const std::filesystem::path& path, const ObjectMetadata& metadata, const Options& options) {
    if (!std::filesystem::exists(path) || std::filesystem::status(path).type() != std::filesystem::file_type::directory) {
        throw std::runtime_error("expected '" + path.string() + "' to be a directory");
    }

    auto vrIt = validate_registry.find(metadata.type);
    if (vrIt == validate_registry.end()) {
        throw std::runtime_error("no registered 'validate' function for object type '" + metadata.type + "' at '" + path.string() + "'");
    }

    try {
        (vrIt->second)(path, metadata, options);
    } catch (std::exception& e) {
        throw std::runtime_error("failed to validate '" + metadata.type + "' object at '" + path.string() + "'; " + std::string(e.what()));
    }
}

/**
 * Validate an object in a subdirectory, using its `OBJECT` file to automatically determine the type.
 *
 * @param path Path to a directory containing an object.
 * @param options Validation options, mostly for input performance.
 */
inline void validate(const std::filesystem::path& path, const Options& options) {
    validate(path, read_object_metadata(path), options);
}

/**
 * Overload of `validate()` with default options.
 *
 * @param path Path to a directory containing an object.
 */
inline void validate(const std::filesystem::path& path) {
    validate(path, Options());
}

}

#endif
