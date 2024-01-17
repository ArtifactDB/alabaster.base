#ifndef CHIHAYA_VALIDATE_HPP
#define CHIHAYA_VALIDATE_HPP

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include "subset.hpp"
#include "combine.hpp"
#include "transpose.hpp"

#include "dense_array.hpp"
#include "sparse_matrix.hpp"
#include "external_hdf5.hpp"
#include "custom_array.hpp"
#include "constant_array.hpp"

#include "dimnames.hpp"
#include "subset_assignment.hpp"

#include "unary_arithmetic.hpp"
#include "unary_comparison.hpp"
#include "unary_logic.hpp"
#include "unary_math.hpp"
#include "unary_special_check.hpp"

#include "binary_arithmetic.hpp"
#include "binary_comparison.hpp"
#include "binary_logic.hpp"

#include "matrix_product.hpp"

#include "utils_public.hpp"

#include <string>
#include <stdexcept>

/**
 * @file validate.hpp
 * @brief Main validation function.
 */

namespace chihaya {

/**
 * @cond
 */
namespace internal {

inline auto default_operation_registry() {
    std::unordered_map<std::string, std::function<ArrayDetails(const H5::Group&, const ritsuko::Version&, Options&)> > registry;
    registry["subset"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return subset::validate(h, v, o); };
    registry["combine"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return combine::validate(h, v, o); };
    registry["transpose"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return transpose::validate(h, v, o); };
    registry["dimnames"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return dimnames::validate(h, v, o); };
    registry["subset assignment"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return subset_assignment::validate(h, v, o); };
    registry["unary arithmetic"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return unary_arithmetic::validate(h, v, o); };
    registry["unary comparison"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return unary_comparison::validate(h, v, o); };
    registry["unary logic"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return unary_logic::validate(h, v, o); };
    registry["unary math"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return unary_math::validate(h, v, o); };
    registry["unary special check"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return unary_special_check::validate(h, v, o); };
    registry["binary arithmetic"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return binary_arithmetic::validate(h, v, o); };
    registry["binary comparison"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return binary_comparison::validate(h, v, o); };
    registry["binary logic"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return binary_logic::validate(h, v, o); };
    registry["matrix product"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return matrix_product::validate(h, v, o); };
    return registry;
}

inline auto default_array_registry() {
    std::unordered_map<std::string, std::function<ArrayDetails(const H5::Group&, const ritsuko::Version&, Options&)> > registry;
    registry["dense array"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return dense_array::validate(h, v, o); };
    registry["sparse matrix"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return sparse_matrix::validate(h, v, o); };
    registry["constant array"] = [](const H5::Group& h, const ritsuko::Version& v, Options& o) -> ArrayDetails { return constant_array::validate(h, v, o); };
    return registry;
}

}
/**
 * @endcond
 */

/**
 * For operations, this function will first search `options.custom_operation_validate_registry` for an available validation function.
 * For arrays, this function will first search `options.custom_array_validate_registry` for an available validation function.
 *
 * @param handle Open handle to a HDF5 group corresponding to a delayed operation or array.
 * @param version Version of the **chihaya** specification.
 * @param options Validation options, possibly containing custom validation functions.
 *
 * @return Details of the array after all delayed operations in `handle` (and its children) have been applied.
 */
inline ArrayDetails validate(const H5::Group& handle, const ritsuko::Version& version, Options& options) {
    auto dtype = ritsuko::hdf5::open_and_load_scalar_string_attribute(handle, "delayed_type");
    ArrayDetails output;

    if (dtype == "array") {
        auto atype = ritsuko::hdf5::open_and_load_scalar_string_attribute(handle, "delayed_array");

        const auto& custom = options.array_validate_registry;
        auto cit = custom.find(atype);
        if (cit != custom.end()) {
            try {
                output = (cit->second)(handle, version, options);
            } catch (std::exception& e) {
                throw std::runtime_error("failed to validate delayed array of type '" + atype + "'; " + std::string(e.what()));
            }

        } else {
            static const auto global = internal::default_array_registry();
            auto git = global.find(atype);
            if (git != global.end()) {
                try {
                    output = (git->second)(handle, version, options);
                } catch (std::exception& e) {
                    throw std::runtime_error("failed to validate delayed array of type '" + atype + "'; " + std::string(e.what()));
                }
            } else if (atype.rfind("custom ", 0) != std::string::npos) {
                try {
                    output = custom_array::validate(handle, version, options);
                } catch (std::exception& e) {
                    throw std::runtime_error("failed to validate delayed array of type '" + atype + "'; " + std::string(e.what()));
                }
            } else if (atype.rfind("external hdf5 ", 0) != std::string::npos && version.lt(1, 1, 0)) {
                try {
                    output = external_hdf5::validate(handle, version, options);
                } catch (std::exception& e) {
                    throw std::runtime_error("failed to validate delayed array of type '" + atype + "'; " + std::string(e.what()));
                }
            } else {
                throw std::runtime_error("unknown array type '" + atype + "'");
            }
        }

    } else if (dtype == "operation") {
        auto otype = ritsuko::hdf5::open_and_load_scalar_string_attribute(handle, "delayed_operation");

        const auto& custom = options.operation_validate_registry;
        auto cit = custom.find(otype);
        if (cit != custom.end()) {
            try {
                output = (cit->second)(handle, version, options);
            } catch (std::exception& e) {
                throw std::runtime_error("failed to validate delayed operation of type '" + otype + "'; " + std::string(e.what()));
            }

        } else {
            static const auto global = internal::default_operation_registry();
            auto git = global.find(otype);
            if (git != global.end()) {
                try {
                    output = (git->second)(handle, version, options);
                } catch (std::exception& e) {
                    throw std::runtime_error("failed to validate delayed operation of type '" + otype + "'; " + std::string(e.what()));
                }
            } else {
                throw std::runtime_error("unknown operation type '" + otype + "'");
            }
        }

    } else {
        throw std::runtime_error("unknown delayed type '" + dtype + "'");
    }

    return output;
}

/**
 * The version is taken from the `delayed_version` attribute of the `handle`.
 * This should be a version string of the form `<MAJOR>.<MINOR>`.
 * For back-compatibility purposes, the string `"1.0.0"` is also allowed, corresponding to version 1.0;
 * and if `delayed_version` is missing, it defaults to `0.99`.
 *
 * @param handle Open handle to a HDF5 group corresponding to a delayed operation or array.
 * @return Version of the **chihaya** specification.
 */
inline ritsuko::Version extract_version(const H5::Group& handle) {
    ritsuko::Version version;

    if (handle.attrExists("delayed_version")) {
        auto ahandle = handle.openAttribute("delayed_version");
        if (!ritsuko::hdf5::is_utf8_string(ahandle)) {
            throw std::runtime_error("expected 'delayed_version' to use a datatype that can be represented by a UTF-8 encoded string");
        }

        auto vstring = ritsuko::hdf5::load_scalar_string_attribute(ahandle);
        if (vstring == "1.0.0") {
            version.major = 1;
        } else {
            version = ritsuko::parse_version_string(vstring.c_str(), vstring.size(), /* skip_patch = */ true);
        }
    } else {
        version.minor = 99;
    }

    return version;
}

/**
 * Validate a delayed operation/array at the specified HDF5 group,
 * 
 * @param handle Open handle to a HDF5 group corresponding to a delayed operation or array.
 * @param options Validation options, see `validate()` for details.
 * @return Details of the array after all delayed operations in `handle` (and its children) have been applied.
 */
inline ArrayDetails validate(const H5::Group& handle, Options& options) {
    return validate(handle, extract_version(handle), options);
}

/**
 * Validate a delayed operation/array at the specified HDF5 group.
 * This simply calls the `validate()` overload for a `H5::Group`.
 * 
 * @param path Path to a HDF5 file.
 * @param name Name of the group inside the file.
 * @param options Validation options, see `validate()` for details.
 *
 * @return Details of the array after all delayed operations have been applied.
 */
inline ArrayDetails validate(const std::string& path, const std::string& name, Options& options) {
    H5::H5File handle(path, H5F_ACC_RDONLY);
    auto ghandle = handle.openGroup(name);
    return validate(ghandle, options);
}

/**
 * Validate a delayed operation/array at the specified HDF5 group.
 * This simply calls the `validate()` overload for a `H5::Group`.
 * 
 * @param path Path to a HDF5 file.
 * @param name Name of the group inside the file.
 *
 * @return Details of the array after all delayed operations have been applied.
 */
inline ArrayDetails validate(const std::string& path, const std::string& name) {
    H5::H5File handle(path, H5F_ACC_RDONLY);
    Options options;
    auto ghandle = handle.openGroup(name);
    return validate(ghandle, options);
}

}

#endif
