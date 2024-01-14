#ifndef CHIHAYA_UTILS_LIST_HPP
#define CHIHAYA_UTILS_LIST_HPP

#include <map>
#include <string>
#include <stdexcept>

#include "H5Cpp.h"
#include "ritsuko/ritsuko.hpp"
#include "ritsuko/hdf5/hdf5.hpp"

#include "utils_misc.hpp"

namespace chihaya {

namespace internal_list {

struct ListDetails {
    size_t length;
    std::map<size_t, std::string> present;
};

inline ListDetails validate(const H5::Group& handle, const ritsuko::Version& version) {
    ListDetails output;

    if (version.lt(1, 1, 0)) {
        auto dtype = ritsuko::hdf5::open_and_load_scalar_string_attribute(handle, "delayed_type");
        if (dtype != "list") {
            throw std::runtime_error("expected 'delayed_type = \"list\"' for a list");
        }
    }

    const char* old_name = "delayed_length";
    const char* new_name = "length";
    const char* actual_name = (version.lt(1, 1, 0) ? old_name : new_name);
    {
        auto lhandle = ritsuko::hdf5::open_attribute(handle, actual_name);
        if (!ritsuko::hdf5::is_scalar(lhandle)) {
            throw std::runtime_error("expected a '" + std::string(actual_name) + "' integer scalar for a list");
        } 

        if (version.lt(1, 1, 0)) {
            if (lhandle.getTypeClass() != H5T_INTEGER) {
                throw std::runtime_error("'" + std::string(actual_name) + "' should be integer");
            }
            int l = 0;
            lhandle.read(H5::PredType::NATIVE_INT, &l);
            if (l < 0) {
                throw std::runtime_error("'" + std::string(actual_name) + "' should be non-negative");
            }
            output.length = l;
        } else {
            if (ritsuko::hdf5::exceeds_integer_limit(lhandle, 64, false)) {
                throw std::runtime_error("datatype of '" + std::string(actual_name) + "' should fit inside a 64-bit unsigned integer");
            }
            output.length = ritsuko::hdf5::load_scalar_numeric_attribute<uint64_t>(lhandle);
        }
    }

    size_t n = handle.getNumObjs();
    if (n > output.length) {
        throw std::runtime_error("more objects in the list than are specified by '" + std::string(actual_name) + "'");
    }
    for (size_t i = 0; i < n; ++i) {
        std::string name = handle.getObjnameByIdx(i);

        // Aaron's cheap and dirty atoi!
        int sofar = 0;
        for (auto c : name) {
            if (c < '0' || c > '9') {
                throw std::runtime_error("'" + name + "' is not a valid name for a list index");
            }
            sofar *= 10;
            sofar += (c - '0'); 
        }

        if (static_cast<size_t>(sofar) >= output.length) {
            throw std::runtime_error("'" + name + "' is out of range for a list"); 
        }
        output.present[sofar] = name;
    }

    return output;
}

}

}

#endif
