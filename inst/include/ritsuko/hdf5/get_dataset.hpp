#ifndef RITSUKO_HDF5_GET_DATASET_HPP
#define RITSUKO_HDF5_GET_DATASET_HPP

#include "H5Cpp.h"
#include <string>

/**
 * @file get_dataset.hpp
 * @brief Quick functions to get a dataset handle.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * @param handle Group containing the dataset.
 * @param name Name of the dataset inside the group.
 * @return Handle to the dataset.
 * An error is raised if `name` does not refer to a dataset. 
 */
inline H5::DataSet get_dataset(const H5::Group& handle, const char* name) {
    if (!handle.exists(name) || handle.childObjType(name) != H5O_TYPE_DATASET) {
        throw std::runtime_error("expected a dataset at '" + std::string(name) + "'");
    }
    return handle.openDataSet(name);
}

/**
 * @param handle Group containing the scalar dataset.
 * @param name Name of the dataset inside the group.
 * @return Handle to a scalar dataset.
 * An error is raised if `name` does not refer to a scalar dataset. 
 */
inline H5::DataSet get_scalar_dataset(const H5::Group& handle, const char* name) {
    auto dhandle = get_dataset(handle, name);
    auto dspace = dhandle.getSpace();
    int ndims = dspace.getSimpleExtentNdims();
    if (ndims != 0) {
        throw std::runtime_error("expected a scalar dataset at '" + std::string(name) + "'");
    }
    return dhandle;
}

}

}

#endif


