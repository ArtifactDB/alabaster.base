#ifndef RITSUKO_HDF5_GET_DIMENSIONS_HPP
#define RITSUKO_HDF5_GET_DIMENSIONS_HPP

#include "H5Cpp.h"
#include <stdexcept>

/**
 * @file get_dimensions.hpp
 * @brief Get the dimensions of a HDF5 dataset.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Get the dimensions of a dataset.
 *
 * @param space The data space of the dataset.
 * @param allow_scalar Whether to allow scalars.
 * 
 * @return Dimensions of the dataset.
 * If `allow_scalar = true`, a zero-length vector is returned in the presence of a scalar dataset, otherwise an error is raised.
 */
inline std::vector<hsize_t> get_dimensions(const H5::DataSpace& space, bool allow_scalar) {
    int ndims = space.getSimpleExtentNdims();
    std::vector<hsize_t> dimensions(ndims);
    if (ndims != 0) {
        space.getSimpleExtentDims(dimensions.data());
    } else if (!allow_scalar) {
        throw std::runtime_error("expected an N-dimensional dataset, got a scalar instead");
    } 
    return dimensions;
}

/**
 * Overload of `get_dimensions()` that accepts a dataset handle.
 *
 * @param handle Handle to a HDF5 dataset.
 * @param allow_scalar Whether to allow scalars.
 * 
 * @return Dimensions of the dataset.
 */
inline std::vector<hsize_t> get_dimensions(const H5::DataSet& handle, bool allow_scalar) {
    return get_dimensions(handle.getSpace(), allow_scalar);
}

/**
 * Overload of `get_dimensions()` that accepts an attribute handle.
 *
 * @param handle Handle to a HDF5 attribute.
 * @param allow_scalar Whether to allow scalars.
 * 
 * @return Dimensions of the attribute.
 */
inline std::vector<hsize_t> get_dimensions(const H5::Attribute& handle, bool allow_scalar) {
    return get_dimensions(handle.getSpace(), allow_scalar);
}


}

}

#endif
