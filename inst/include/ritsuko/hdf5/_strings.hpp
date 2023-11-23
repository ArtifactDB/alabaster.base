#ifndef RITSUKO_HDF5_STRINGS_HPP
#define RITSUKO_HDF5_STRINGS_HPP

#include "H5Cpp.h"

namespace ritsuko {

namespace hdf5 {

inline size_t find_string_length(const char* ptr, size_t max) {
    size_t j = 0;
    for (; j < max && ptr[j] != '\0'; ++j) {}
    return j;
}

struct VariableStringCleaner {
    VariableStringCleaner(hid_t did, hid_t mid, char** buffer) : did(did), mid(mid), buffer(buffer) {}
    ~VariableStringCleaner() {
        H5Dvlen_reclaim(did, mid, H5P_DEFAULT, buffer);
    }
    hid_t did, mid;
    char** buffer; 
};

}

}

#endif
