#ifndef TAKANE_ARRAY_HPP
#define TAKANE_ARRAY_HPP

#include "H5Cpp.h"
#include "ritsuko/hdf5/hdf5.hpp"

namespace takane {

/**
 * @namespace takane::array
 * @brief Definitions for abstract arrays.
 */
namespace array {

/**
 * Type of the data values in the array.
 */
enum Type {
    INTEGER,
    NUMBER,
    BOOLEAN,
    STRING
};

/**
 * @cond
 */
template<class Dimensions_>
void check_dimnames(const H5::H5File& handle, const std::string& dimnames_group, const Dimensions_& dimensions) try {
    if (!handle.exists(dimnames_group) || handle.childObjType(dimnames_group) != H5O_TYPE_GROUP) {
        throw std::runtime_error("expected a group");
    }
    auto nhandle = handle.openGroup(dimnames_group);

    for (size_t i = 0, ndim = dimensions.size(); i < ndim; ++i) {
        std::string dim_name = std::to_string(i);
        if (!nhandle.exists(dim_name)) {
            continue;
        }

        if (nhandle.childObjType(dim_name) != H5O_TYPE_DATASET) {
            throw std::runtime_error("expected '" + dim_name + "' to be a dataset");
        }
        auto dnhandle = nhandle.openDataSet(dim_name);

        if (dnhandle.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("expected '" + dim_name + "' to be a string dataset");
        }
        if (ritsuko::hdf5::get_1d_length(dnhandle.getSpace(), false) != dimensions[i]) {
            throw std::runtime_error("expected '" + dim_name + "' to have length equal to the corresponding dimension");
        }
    }
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate dimnames at '/" + dimnames_group + "'; " + std::string(e.what()));
}
/**
 * @endcond
 */
}

}

#endif
