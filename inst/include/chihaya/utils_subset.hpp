#ifndef CHIHAYA_UTILS_SUBSET_HPP
#define CHIHAYA_UTILS_SUBSET_HPP

#include "H5Cpp.h"
#include "ritsuko/hdf5/hdf5.hpp"

#include <vector>
#include <stdexcept>

#include "utils_list.hpp"
#include "utils_misc.hpp"

namespace chihaya {

namespace internal_subset {

template<typename Index_>
void validate_indices(const H5::DataSet& dhandle, size_t len, size_t extent) {
    ritsuko::hdf5::Stream1dNumericDataset<Index_> stream(&dhandle, len, 1000000);
    for (size_t i = 0; i < len; ++i, stream.next()) {
        auto b = stream.get();
        if (b < 0) {
            throw std::runtime_error("indices should be non-negative");
        }
        if (static_cast<size_t>(b) >= extent) {
            throw std::runtime_error("indices out of range");
        }
    }
}

inline std::vector<std::pair<size_t, size_t> > validate_index_list(const H5::Group& ihandle, const std::vector<size_t>& seed_dims, const ritsuko::Version& version) {
    internal_list::ListDetails list_params;
    try {
        list_params = internal_list::validate(ihandle, version);
    } catch (std::exception& e) {
        throw std::runtime_error("failed to load 'index' list; " + std::string(e.what()));
    }

    if (list_params.length != seed_dims.size()) {
        throw std::runtime_error("length of 'index' should be equal to number of dimensions in 'seed'");
    }

    std::vector<std::pair<size_t, size_t> > collected;

    for (const auto& p : list_params.present) {
        try {
            auto dhandle = ritsuko::hdf5::open_dataset(ihandle, p.second.c_str());
            auto len = ritsuko::hdf5::get_1d_length(dhandle, false);

            if (version.lt(1, 1, 0)) {
                if (dhandle.getTypeClass() != H5T_INTEGER) {
                    throw std::runtime_error("expected an integer dataset");
                }
                validate_indices<int>(dhandle, len, seed_dims[p.first]);
            } else {
                if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 64, false)) {
                    throw std::runtime_error("datatype should be exactly represented by a 64-bit unsigned integer");
                }
                validate_indices<uint64_t>(dhandle, len, seed_dims[p.first]);
            }

            collected.emplace_back(p.first, len);
        } catch (std::exception& e) {
            throw std::runtime_error("failed to validate 'index/" + p.second + "'; " + std::string(e.what()));
        }
    }

    return collected;
}

}

}

#endif
