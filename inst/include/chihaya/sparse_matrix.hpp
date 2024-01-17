#ifndef CHIHAYA_SPARSE_MATRIX_HPP
#define CHIHAYA_SPARSE_MATRIX_HPP

#include "H5Cpp.h"
#include "ritsuko/hdf5/hdf5.hpp"

#include <vector>
#include <cstdint>

#include "utils_public.hpp"
#include "utils_misc.hpp"
#include "utils_type.hpp"
#include "utils_dimnames.hpp"

/**
 * @file sparse_matrix.hpp
 * @brief Validation for compressed sparse column matrices. 
 */

namespace chihaya {

/**
 * @namespace chihaya::sparse_matrix
 * @brief Namespace for sparse matrices.
 */
namespace sparse_matrix {

/**
 * @cond
 */
namespace internal {

template<typename Index_>
void validate_indices(const H5::DataSet& ihandle, const std::vector<uint64_t>& indptrs, size_t primary, size_t secondary, bool csc) {
    ritsuko::hdf5::Stream1dNumericDataset<Index_> stream(&ihandle, indptrs.back(), 1000000);

    for (size_t p = 0; p < primary; ++p) {
        auto start = indptrs[p];
        auto end = indptrs[p + 1];
        if (start > end) {
            throw std::runtime_error("entries of 'indptr' must be sorted");
        }

        // Checking for sortedness and good things.
        Index_ previous;
        for (auto x = start; x < end; ++x, stream.next()) {
            auto i = stream.get();
            if (i < 0) {
                throw std::runtime_error("entries of 'indices' should be non-negative");
            }
            if (x > start && i <= previous) {
                throw std::runtime_error("'indices' should be strictly increasing within each " + (csc ? std::string("column") : std::string("row")));
            }
            if (static_cast<size_t>(i) >= secondary) {
                throw std::runtime_error("entries of 'indices' should be less than the number of " + (csc ? std::string("row") : std::string("column")) + "s");
            }
            previous = i;
        }
    }
}

}
/**
 * @endcond
 */

/**
 * @param handle An open handle on a HDF5 group representing a sparse matrix.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options.
 * 
 * @return Details of the sparse matrix.
 * Otherwise, if the validation failed, an error is raised.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, [[maybe_unused]] Options& options) {
    std::vector<uint64_t> dims(2);
    ArrayType array_type;

    {
        auto shandle = ritsuko::hdf5::open_dataset(handle, "shape");
        auto len = ritsuko::hdf5::get_1d_length(shandle, false);
        if (len != 2) {
            throw std::runtime_error("'shape' should have length 2");
        }

        if (version.lt(1, 1, 0)) {
            if (shandle.getTypeClass() != H5T_INTEGER) {
                throw std::runtime_error("'shape' should be integer");
            }
            std::vector<int> dims_tmp(2);
            shandle.read(dims_tmp.data(), H5::PredType::NATIVE_INT);
            if (dims_tmp[0] < 0 || dims_tmp[1] < 0) {
                throw std::runtime_error("'shape' should contain non-negative values");
            }
            std::copy(dims_tmp.begin(), dims_tmp.end(), dims.begin());
        } else {
            if (ritsuko::hdf5::exceeds_integer_limit(shandle, 64, false)) {
                throw std::runtime_error("'shape' should have a datatype that can fit into a 64-bit unsigned integer");
            }
            shandle.read(dims.data(), H5::PredType::NATIVE_UINT64);
        }
    }

    size_t nnz;
    {
        auto dhandle = ritsuko::hdf5::open_dataset(handle, "data");

        try {
            nnz = ritsuko::hdf5::get_1d_length(dhandle, false);

            if (version.lt(1, 1, 0)) {
                array_type = internal_type::translate_type_0_0(dhandle.getTypeClass());
                if (internal_type::is_boolean(dhandle)) {
                    array_type = BOOLEAN;
                }
            } else {
                auto type = ritsuko::hdf5::open_and_load_scalar_string_attribute(dhandle, "type");
                array_type = internal_type::translate_type_1_1(type);
                internal_type::check_type_1_1(dhandle, array_type);
            }

            if (array_type != INTEGER && array_type != BOOLEAN && array_type != FLOAT) {
                throw std::runtime_error("dataset should be integer, float or boolean");
            }

            internal_misc::validate_missing_placeholder(dhandle, version);
        } catch (std::exception& e) {
            throw std::runtime_error("failed to validate 'data'; " + std::string(e.what()));
        }
    }

    if (!options.details_only) {
        bool csc = true;
        if (!version.lt(1, 1, 0)) {
            auto bhandle = ritsuko::hdf5::open_dataset(handle, "by_column");
            if (!ritsuko::hdf5::is_scalar(bhandle)) {
                throw std::runtime_error("'by_column' should be a scalar");
            }
            if (ritsuko::hdf5::exceeds_integer_limit(bhandle, 8, true)) {
                throw std::runtime_error("datatype of 'by_column' should fit into an 8-bit signed integer");
            }
            csc = (ritsuko::hdf5::load_scalar_numeric_dataset<int8_t>(bhandle) != 0);
        }

        {
            auto ihandle = ritsuko::hdf5::open_dataset(handle, "indices");

            if (version.lt(1, 1, 0)) {
                if (ihandle.getTypeClass() != H5T_INTEGER) {
                    throw std::runtime_error("'indices' should be integer");
                }
            } else {
                if (ritsuko::hdf5::exceeds_integer_limit(ihandle, 64, false)) {
                    throw std::runtime_error("datatype of 'indices' should fit into a 64-bit unsigned integer");
                }
            }

            if (nnz != ritsuko::hdf5::get_1d_length(ihandle, false)) {
                throw std::runtime_error("'indices' and 'data' should have the same length");
            }

            auto iphandle = ritsuko::hdf5::open_dataset(handle, "indptr");
            if (version.lt(1, 1, 0)) {
                if (iphandle.getTypeClass() != H5T_INTEGER) {
                    throw std::runtime_error("'indptr' should be integer");
                }
            } else {
                if (ritsuko::hdf5::exceeds_integer_limit(iphandle, 64, false)) {
                    throw std::runtime_error("datatype of 'indptr' should fit into a 64-bit unsigned integer");
                }
            }

            auto primary = (csc ? dims[1] : dims[0]);
            auto secondary = (csc ? dims[0] : dims[1]);
            if (ritsuko::hdf5::get_1d_length(iphandle, false) != static_cast<size_t>(primary + 1)) {
                throw std::runtime_error("'indptr' should have length equal to the number of " + (csc ? std::string("columns") : std::string("rows")) + " plus 1");
            }
            std::vector<uint64_t> indptrs(primary + 1);
            iphandle.read(indptrs.data(), H5::PredType::NATIVE_UINT64);
            if (indptrs[0] != 0) {
                throw std::runtime_error("first entry of 'indptr' should be 0 for a sparse matrix");
            }
            if (indptrs.back() != static_cast<uint64_t>(nnz)) {
                throw std::runtime_error("last entry of 'indptr' should be equal to the length of 'data'");
            }

            if (version.lt(1, 1, 0)) {
                internal::validate_indices<int>(ihandle, indptrs, primary, secondary, csc);
            } else {
                internal::validate_indices<uint64_t>(ihandle, indptrs, primary, secondary, csc);
            }
        }

        // Validating dimnames.
        if (handle.exists("dimnames")) {
            internal_dimnames::validate(handle, dims, version);
        }
    }

    return ArrayDetails(array_type, std::vector<size_t>(dims.begin(), dims.end()));
}

}

}

#endif
