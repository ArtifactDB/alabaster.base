#ifndef TAKANE_SIMPLE_LIST_HPP
#define TAKANE_SIMPLE_LIST_HPP

#include <string>
#include <stdexcept>
#include <filesystem>

#include "uzuki2/uzuki2.hpp"
#include "byteme/byteme.hpp"

#include "utils_public.hpp"

/**
 * @file simple_list.hpp
 * @brief Validation for simple lists.
 */

namespace takane {

/**
 * @cond
 */
void validate(const std::filesystem::path&, const Options&);
/**
 * @endcond
 */

/**
 * @namespace takane::simple_list
 * @brief Definitions for simple lists.
 */
namespace simple_list {

/**
 * @param path Path to the directory containing the simple list.
 * @param options Validation options, typically for reading performance.
 */
inline void validate(const std::filesystem::path& path, const Options& options) try {
    auto other_dir = path / "other_contents";
    int num_external = 0;
    if (std::filesystem::exists(other_dir)) {
        auto status = std::filesystem::status(other_dir);
        if (status.type() != std::filesystem::file_type::directory) {
            throw std::runtime_error("expected 'other_contents' to be a directory");
        } 

        for (const auto& entry : std::filesystem::directory_iterator(other_dir)) {
            try {
                ::takane::validate(entry.path().string(), options);
            } catch (std::exception& e) {
                throw std::runtime_error("failed to validate external list object at '" + std::filesystem::relative(entry.path(), path).string() + "'; " + std::string(e.what()));
            }
            ++num_external;
        }
    }

    {
        auto candidate = path / "list_contents.json.gz";
        if (std::filesystem::exists(candidate)) {
            uzuki2::json::Options opt;
            opt.parallel = options.parallel_reads;
            byteme::SomeFileReader gzreader(candidate.string());
            uzuki2::json::validate(gzreader, num_external, opt);
            return;
        } 
    }

    {
        auto candidate = path / "list_contents.h5";
        if (std::filesystem::exists(candidate)) {
            uzuki2::hdf5::validate(candidate.string(), "simple_list", num_external);
            return;
        } 
    }

    throw std::runtime_error("could not determine format from the file names");
} catch (std::exception& e) {
    throw std::runtime_error("failed to validate a 'simple_list' at '" + path.string() + "'; " + std::string(e.what()));
}

/**
 * @param path Path to the directory containing the simple list.
 * @param options Validation options, typically for reading performance.
 * @return The number of list elements.
 */
inline size_t height(const std::filesystem::path& path, const Options& options) {
    {
        auto candidate = path / "list_contents.h5";
        if (std::filesystem::exists(candidate)) {
            H5::H5File handle(candidate, H5F_ACC_RDONLY);
            auto lhandle = handle.openGroup("simple_list");
            auto vhandle = lhandle.openGroup("data");
            return vhandle.getNumObjs();
        } 
    }

    // Not much choice but to parse the entire list here. We do so using the
    // dummy, which still has enough self-awareness to hold its own length.
    auto other_dir = path / "other_contents";
    int num_external = 0;
    if (std::filesystem::exists(other_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(other_dir)) {
            (void)entry; // silence compiler warnings about unused variables.
            ++num_external;
        }
    }

    uzuki2::json::Options opt;
    opt.parallel = options.parallel_reads;
    auto candidate = path / "list_contents.json.gz";
    byteme::SomeFileReader gzreader(candidate.string());
    uzuki2::DummyExternals ext(num_external);
    auto ptr = uzuki2::json::parse<uzuki2::DummyProvisioner>(gzreader, std::move(ext), std::move(opt));
    return reinterpret_cast<const uzuki2::List*>(ptr.get())->size();
}

}

}

#endif
