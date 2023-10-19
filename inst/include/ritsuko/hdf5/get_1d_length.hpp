#ifndef RITSUKO_HDF5_GET_1D_LENGTH_HPP
#define RITSUKO_HDF5_GET_1D_LENGTH_HPP

#include "H5Cpp.h"
#include <stdexcept>

/**
 * @file get_1d_length.hpp
 * @brief Get the length of a 1-dimensional HDF5 dataset.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Get the length of a 1-dimensional HDF5 dataset.
 *
 * @param space The data space of the dataset.
 * @param allow_scalar Whether to allow scalars.
 * @param name Name of the dataset, to use for error messages.
 * 
 * @return Length of the dataset, i.e., the extent of its single dimension.
 * If `allow_scalar = true`, zero is returned in the presence of a scalar dataset, otherwise an error is raised.
 */
inline hsize_t get_1d_length(const H5::DataSpace& space, bool allow_scalar, const char* name) {
    int ndims = space.getSimpleExtentNdims();
    if (ndims == 0 && allow_scalar) {
        return 0;
    }
    if (ndims != 1) {
        throw std::runtime_error("expected a 1-dimensional dataset at '" + std::string(name) + "'");
    }

    hsize_t dims;
    space.getSimpleExtentDims(&dims);
    return dims;
}

}

}

#endif
