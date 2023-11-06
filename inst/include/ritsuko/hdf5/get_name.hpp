#ifndef RITSUKO_HDF5_GET_NAME_HPP
#define RITSUKO_HDF5_GET_NAME_HPP

#include "H5Cpp.h"
#include <string>
#include <vector>

/**
 * @file get_name.hpp
 * @brief Get the name of a HDF5 object.
 */

namespace ritsuko {

namespace hdf5 {

/**
 * Get the name of a HDF5 object from its handle, usually for printing informative error messages.
 * @tparam Handle_ Type of HDF5 handle, usually a `Group` or a `DataSet`.
 * @param handle Handle to a HDF5 object.
 * @return Name of the HDF5 object inside the file.
 */
template<class Handle_>
std::string get_name(const Handle_& handle) {
    size_t len = H5Iget_name(handle.getId(), NULL, 0);
    std::vector<char> buffer(len);
    H5Iget_name(handle.getId(), buffer.data(), len+1);
    return std::string(buffer.begin(), buffer.end());
}

}

}

#endif
