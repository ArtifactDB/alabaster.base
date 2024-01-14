#ifndef TAKANE_UTILS_DIMNAMES_HPP
#define TAKANE_UTILS_DIMNAMES_HPP

#include "H5Cpp.h"
#include "ritsuko/hdf5/hdf5.hpp"
#include "ritsuko/ritsuko.hpp"
#include <string>
#include <stdexcept>
#include "utils_list.hpp"

namespace chihaya {

namespace internal_dimnames {

template<class V>
void validate(const H5::Group& handle, const V& dimensions, const ritsuko::Version& version) try {
    if (handle.childObjType("dimnames") != H5O_TYPE_GROUP) {
        throw std::runtime_error("expected a group at 'dimnames'");
    }
    auto ghandle = handle.openGroup("dimnames");
    auto list_params = internal_list::validate(ghandle, version);

    if (list_params.length != dimensions.size()) {
        throw std::runtime_error("length of 'dimnames' list should be equal to seed dimensionality");
    }

    for (const auto& p : list_params.present) {
        auto current = ritsuko::hdf5::open_dataset(ghandle, p.second.c_str());
        if (current.getSpace().getSimpleExtentNdims() != 1 || current.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("each entry of 'dimnames' should be a 1-dimensional string dataset");
        }

        auto len = ritsuko::hdf5::get_1d_length(current, false);
        if (len != static_cast<hsize_t>(dimensions[p.first])) {
            throw std::runtime_error("each entry of 'dimnames' should have length equal to the extent of its corresponding dimension");
        }

        ritsuko::hdf5::validate_1d_string_dataset(current, len, 1000000);
    }
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate the 'dimnames'; " + std::string(e.what()));
}

}

}

#endif
