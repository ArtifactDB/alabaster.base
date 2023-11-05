#ifndef TAKANE_ARRAY_HPP
#define TAKANE_ARRAY_HPP

#include "H5Cpp.h"
#include "ritsuko/hdf5/hdf5.hpp"
#include "ritsuko/ritsuko.hpp"

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
inline void check_data(const H5::DataSet& dhandle, Type type, const ritsuko::Version& version, int old_version) try {
    if (type == array::Type::INTEGER) {
        if (version.major) {
            if (ritsuko::hdf5::load_scalar_string_attribute(dhandle, "type") != "integer") {
                throw std::runtime_error("expected 'type' attribute to be 'integer'");
            }
            if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 32, true)) {
                throw std::runtime_error("expected datatype to be a subset of a 32-bit signed integer");
            }
        } else {
            if (dhandle.getTypeClass() != H5T_INTEGER) {
                throw std::runtime_error("expected an integer type class");
            }
        }

    } else if (type == array::Type::BOOLEAN) {
        if (version.major) {
            if (ritsuko::hdf5::load_scalar_string_attribute(dhandle, "type") != "boolean") {
                throw std::runtime_error("expected 'type' attribute to be 'boolean'");
            }
            if (ritsuko::hdf5::exceeds_integer_limit(dhandle, 32, true)) {
                throw std::runtime_error("expected datatype to be a subset of a 32-bit signed integer");
            }
        } else {
            if (dhandle.getTypeClass() != H5T_INTEGER) {
                throw std::runtime_error("expected an integer type class");
            }
        }

    } else if (type == array::Type::NUMBER) {
        if (version.major) {
            if (ritsuko::hdf5::load_scalar_string_attribute(dhandle, "type") != "number") {
                throw std::runtime_error("expected 'type' attribute to be 'number'");
            }
            if (ritsuko::hdf5::exceeds_float_limit(dhandle, 64)) {
                throw std::runtime_error("expected datatype to be a subset of a 64-bit float");
            }
        } else {
            auto tclass = dhandle.getTypeClass();
            if (tclass != H5T_FLOAT && tclass != H5T_INTEGER) {
                throw std::runtime_error("expected an integer or floating-point type class");
            }
        }

    } else if (type == array::Type::STRING) {
        if (version.major) {
            if (ritsuko::hdf5::load_scalar_string_attribute(dhandle, "type") != "string") {
                throw std::runtime_error("expected 'type' attribute to be 'string'");
            }
        }
        if (dhandle.getTypeClass() != H5T_STRING) {
            throw std::runtime_error("expected a string type class");
        }

    } else {
        throw std::runtime_error("not-yet-supported array type (" + std::to_string(static_cast<int>(type)) + ")");
    }

    if (version.major || old_version >= 2) {
        const char* missing_attr = "missing-value-placeholder";
        if (dhandle.attrExists(missing_attr)) {
            ritsuko::hdf5::get_missing_placeholder_attribute(dhandle, missing_attr, /* type_class_only = */ type == array::Type::STRING);
        }
    }
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate dataset at '" + ritsuko::hdf5::get_name(dhandle) + "'; " + std::string(e.what()));
}

inline void check_dimnames(const H5::DataSet& handle, size_t expected_dim) try {
    if (handle.getTypeClass() != H5T_STRING) {
        throw std::runtime_error("expected a string dataset");
    }
    if (ritsuko::hdf5::get_1d_length(handle.getSpace(), false) != expected_dim) {
        throw std::runtime_error("expected dataset to have length equal to the corresponding dimension extent (" + std::to_string(expected_dim) + ")");
    }
} catch (std::exception& e) {
    throw std::runtime_error("failed to check dimnames at '" + ritsuko::hdf5::get_name(handle) + "'; " + std::string(e.what()));
}

template<class Dimensions_>
void check_dimnames(const H5::H5File& handle, const std::string& dimnames_group, const Dimensions_& dimensions) try {
    if (dimnames_group.empty()) {
        return;
    }
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
        check_dimnames(nhandle.openDataSet(dim_name), dimensions[i]);
    }
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate dimnames at '" + dimnames_group + "'; " + std::string(e.what()));
}

template<class Host_, class Dimensions_>
void check_dimnames2(const H5::H5File& handle, const Host_& host, const Dimensions_& dimensions, bool reverse = false) try {
    if (!host.attrExists("dimension-names")) {
        return;
    }

    auto dimnames = host.openAttribute("dimension-names");
    if (dimnames.getTypeClass() != H5T_STRING) {
        throw std::runtime_error("'" + dimnames.getName() + "' attribute should have a string datatype");
    }
    
    size_t ndim = dimensions.size();
    if (ritsuko::hdf5::get_1d_length(dimnames.getSpace(), false) != ndim) {
        throw std::runtime_error("'" + dimnames.getName() + "' attribute should have length equal to the number of dimensions (" + std::to_string(ndim) + ")");
    }

    ritsuko::hdf5::load_1d_string_attribute(
        dimnames, 
        dimensions.size(), 
        [&](size_t i, const char* start, size_t len) {
            if (len) {
                auto x = std::string(start, start + len); // need to allocate to ensure correct null termination.
                auto dhandle = ritsuko::hdf5::get_dataset(handle, x.c_str());
                check_dimnames(dhandle, dimensions[reverse ? ndim - i - 1 : i]);
            }
        }
    );
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate dimnames for '" + ritsuko::hdf5::get_name(host) + "'; " + std::string(e.what()));
}
/**
 * @endcond
 */
}

}

#endif
