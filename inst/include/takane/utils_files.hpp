#ifndef TAKANE_UTILS_FILES_HPP
#define TAKANE_UTILS_FILES_HPP

#include <string>
#include <stdexcept>
#include <filesystem>
#include <array>

#include "utils_other.hpp"
#include "utils_json.hpp"
#include "byteme/byteme.hpp"

namespace takane {

namespace internal_files {

template<class Reader_ = byteme::RawFileReader>
void check_signature(const std::filesystem::path& path, const char* expected, size_t len, const char* msg) {
    auto reader = internal_other::open_reader<Reader_>(path, /* buffer_size = */ len);
    byteme::PerByte<> pb(&reader);
    bool okay = pb.valid();
    for (size_t i = 0; i < len; ++i) {
        if (!okay) {
            throw std::runtime_error("incomplete " + std::string(msg) + " file signature for '" + path.string() + "'");
        }
        if (pb.get() != expected[i]) {
            throw std::runtime_error("incorrect " + std::string(msg) + " file signature for '" + path.string() + "'");
        }
        okay = pb.advance();
    }
}

template<class Reader_ = byteme::RawFileReader>
void check_signature(const std::filesystem::path& path, const unsigned char* expected, size_t len, const char* msg) {
    auto reader = internal_other::open_reader<Reader_>(path, /* buffer_size = */ len);
    byteme::PerByte<unsigned char> pb(&reader);
    bool okay = pb.valid();
    for (size_t i = 0; i < len; ++i) {
        if (!okay) {
            throw std::runtime_error("incomplete " + std::string(msg) + " file signature for '" + path.string() + "'");
        }
        if (pb.get() != expected[i]) {
            throw std::runtime_error("incorrect " + std::string(msg) + " file signature for '" + path.string() + "'");
        }
        okay = pb.advance();
    }
}

inline void check_gzip_signature(const std::filesystem::path& path) {
    std::array<unsigned char, 2> gzmagic { 0x1f, 0x8b };
    check_signature(path, gzmagic.data(), gzmagic.size(), "GZIP");
}

inline void extract_signature(const std::filesystem::path& path, unsigned char* store, size_t len) {
    auto reader = internal_other::open_reader<byteme::RawFileReader>(path, /* buffer_size = */ len);
    byteme::PerByte<unsigned char> pb(&reader);
    bool okay = pb.valid();
    for (size_t i = 0; i < len; ++i) {
        if (!okay) {
            throw std::runtime_error("file at '" + path.string() + "' is too small to extract a signature of length " + std::to_string(len));
        }
        store[i] = pb.get();
        okay = pb.advance();
    }
}

inline bool is_indexed(const internal_json::JsonObjectMap& objmap) {
    auto iIt = objmap.find("indexed");
    if (iIt == objmap.end()) {
        return false;
    }

    const auto& val = iIt->second;
    if (val->type() != millijson::BOOLEAN) {
        throw std::runtime_error("property should be a JSON boolean");
    }

    return reinterpret_cast<const millijson::Boolean*>(val.get())->value;
}

inline void check_sequence_type(const internal_json::JsonObjectMap& objmap, const char* msg) {
    auto sIt = objmap.find("sequence_type");
    if (sIt == objmap.end()) {
        throw std::runtime_error("expected a '" + std::string(msg) + ".sequence_type' property");
    }

    const auto& val = sIt->second;
    if (val->type() != millijson::STRING) {
        throw std::runtime_error("'" + std::string(msg) + ".sequence_type' property should be a JSON string");
    }

    const auto& stype = reinterpret_cast<const millijson::String*>(val.get())->value;
    if (stype != "RNA" && stype != "DNA" && stype != "AA" && stype != "custom") {
        throw std::runtime_error("unsupported value '" + stype + "' for the '" + std::string(msg) + ".sequence_type' property");
    }
}

}

}

#endif
