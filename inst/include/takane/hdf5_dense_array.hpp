#ifndef TAKANE_HDF5_DENSE_ARRAY_HPP
#define TAKANE_HDF5_DENSE_ARRAY_HPP

#include "ritsuko/hdf5/hdf5.hpp"
#include "ritsuko/ritsuko.hpp"

#include "array.hpp"

#include <vector>
#include <string>
#include <stdexcept>

/**
 * @file hdf5_dense_array.hpp
 * @brief Validation for HDF5 dense arrays.
 */

namespace takane {

/**
 * @namespace takane::hdf5_dense_array
 * @brief Definitions for HDF5 dense arrays.
 */
namespace hdf5_dense_array {

/**
 * @brief Parameters for validating a HDF5 dense array file.
 */
struct Parameters {
    /**
     * @param dataset Name of the dataset.
     * @param dimensions Dimensions of the array.
     */
    Parameters(std::string dataset, std::vector<size_t> dimensions) : dataset(std::move(dataset)), dimensions(std::move(dimensions)) {}

    /**
     * Name of the dataset in the HDF5 file.
     */
    std::string dataset;

    /**
     * Expected dimensions of the dense array.
     * Dimensions are sorted from the fastest changing to the slowest changing.
     */
    std::vector<size_t> dimensions;

    /**
     * Expected type of the dense array.
     */
    array::Type type = array::Type::INTEGER;

    /**
     * Name of the group containing the dimension names.
     * If empty, it is assumed that no dimension names are present.
     * Ignored if a `version` attribute is present on the HDF5 dataset at `dataset`.
     */
    std::string dimnames_group;

    /**
     * Version of this file specification.
     * Ignored if a `version` attribute is present on the HDF5 dataset at `dataset`.
     */
    int version = 2;
};

/**
 * Validate a file containing a HDF5 dense array.
 * An error is raised if the file does not meet the specifications.
 *
 * @param handle Handle to the file.
 * @param params Validation parameters.
 */
inline void validate(const H5::H5File& handle, const Parameters& params) {
    const auto& dataset = params.dataset;
    if (!handle.exists(dataset) || handle.childObjType(dataset) != H5O_TYPE_DATASET) {
        throw std::runtime_error("expected a '" + dataset + "' dataset");
    }
    auto dhandle = handle.openDataSet(dataset);

    ritsuko::Version version;
    if (dhandle.attrExists("version")) {
        auto vstring = ritsuko::hdf5::load_scalar_string_attribute(dhandle, "version");
        version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
        if (version.major != 1) {
            throw std::runtime_error("unsupported version '" + vstring + "' for the '" + params.dataset + "' dataset");
        }
    }

    {
        const auto& dimensions = params.dimensions;
        auto dspace = dhandle.getSpace();

        size_t ndims = dspace.getSimpleExtentNdims();
        if (ndims != dimensions.size()) {
            throw std::runtime_error("unexpected number of dimensions for '" + dataset + "'");
        }

        std::vector<hsize_t> dims(ndims);
        dspace.getSimpleExtentDims(dims.data());

        for (size_t d = 0; d < ndims; ++d) {
            auto expected = dimensions[d];
            auto observed = dims[ndims - d - 1];
            if (observed != expected) {
                throw std::runtime_error("unexpected dimension extent for '" + dataset + "' (expected " + std::to_string(expected) + ", got " + std::to_string(observed) + ")");
            }
        }
    }

    array::check_data(dhandle, params.type, version, params.version);

    if (version.major) {
        array::check_dimnames2(handle, dhandle, params.dimensions, true);
    } else {
        array::check_dimnames(handle, params.dimnames_group, params.dimensions);
    }
}

/**
 * Overload for `hdf5_dense_array::validate()` that accepts a file path.
 *
 * @param path Path to the file.
 * @param params Validation parameters.
 */
inline void validate(const char* path, const Parameters& params) {
    H5::H5File handle(path, H5F_ACC_RDONLY);
    validate(handle, params);
}

}

}

#endif
