#include "Rcpp.h"

#include "ritsuko/hdf5/vls/vls.hpp"
#include "H5Cpp.h"
#include <cstdlib>

// Copied from https://github.com/grimbough/rhdf5/blob/devel/src/myhdf5.h.
#define STRSXP_2_HID(x) std::atoll(CHAR(Rf_asChar(x)))

//[[Rcpp::export(rng=false)]]
SEXP dump_vls_heap(SEXP hid, Rcpp::CharacterVector values, Rcpp::IntegerVector lengths, size_t buffer_size) {
    H5::DataSet handle(STRSXP_2_HID(hid));
    auto hspace = handle.getSpace();
    H5::DataSpace mspace;

    std::vector<uint8_t> buffer;
    size_t vlen = values.size();
    hsize_t last = 0;
    auto save_buffer = [&]() -> void {
        hsize_t my_size = buffer.size();
        mspace.setExtentSimple(1, &my_size);
        mspace.selectAll();
        hspace.selectHyperslab(H5S_SELECT_SET, &my_size, &last);
        handle.write(buffer.data(), H5::PredType::NATIVE_UINT8, mspace, hspace);
    };

    for (size_t i = 0; i < vlen; ++i) {
        Rcpp::String val(values[i]);
        size_t len = lengths[i];
        auto cptr = reinterpret_cast<const unsigned char*>(val.get_cstring());
        buffer.insert(buffer.end(), cptr, cptr + len);
        if (buffer.size() < buffer_size) {
            continue;
        }

        save_buffer();
        last += buffer.size();
        buffer.clear();
    }

    if (!buffer.empty()) {
        save_buffer();
    }

    return R_NilValue;
}

//[[Rcpp::export(rng=false)]]
SEXP create_vls_pointer_dataset(SEXP gid, std::string name, Rcpp::IntegerVector dimensions, Rcpp::IntegerVector chunks, int compress, bool scalar) {
    H5::Group ghandle(STRSXP_2_HID(gid));
    auto ptype = ritsuko::hdf5::vls::define_pointer_datatype<uint64_t, uint64_t>();

    if (scalar) {
        ghandle.createDataSet(name, ptype, H5S_SCALAR);
    } else {
        std::vector<hsize_t> dims(dimensions.begin(), dimensions.end());
        H5::DataSpace dspace(dims.size(), dims.data());

        H5::DSetCreatPropList cplist;
        H5Pset_obj_track_times(cplist.getId(), 0); // hm, don't know the C++ version of this.
        cplist.setFillTime(H5D_FILL_TIME_NEVER);

        if (compress > 0) {
            cplist.setShuffle();
            cplist.setDeflate(compress);
            std::vector<hsize_t> ch(chunks.begin(), chunks.end());
            if (ch.size() != dims.size()) {
                throw std::runtime_error("'dimensions' and 'chunks' should have the same length");
            }
            cplist.setChunk(dims.size(), ch.data());
        } else {
            cplist = H5::DSetCreatPropList::DEFAULT;
        }

        ghandle.createDataSet(name, ptype, dspace, cplist);
    }

    return R_NilValue;
}

//[[Rcpp::export(rng=false)]]
SEXP dump_vls_pointers(SEXP pid, Rcpp::IntegerVector lengths, size_t buffer_size) {
    H5::DataSet handle(STRSXP_2_HID(pid));
    auto space = handle.getSpace();

    hsize_t ndims = space.getSimpleExtentNdims();
    std::vector<hsize_t> dims(ndims); 
    space.getSimpleExtentDims(dims.data());
    auto block = ritsuko::hdf5::pick_nd_block_dimensions(handle.getCreatePlist(), dims, buffer_size);

    ritsuko::hdf5::IterateNdDataset iter(dims, block);
    std::vector<ritsuko::hdf5::vls::Pointer<uint64_t, uint64_t> > buffer;
    auto dtype = ritsuko::hdf5::vls::define_pointer_datatype<uint64_t, uint64_t>();

    std::vector<uint64_t> cumulative;
    cumulative.reserve(lengths.size());
    uint64_t start = 0;
    for (auto l : lengths) {
        cumulative.push_back(start);
        start += l;
    }

    std::vector<hsize_t> jumps(ndims, 1);
    hsize_t jump = 1;
    for (size_t d = 0; d < ndims; ++d) {
        jumps[d] = jump;
        jump *= dims[d];
    }
    std::reverse(jumps.begin(), jumps.end()); // reversing as HDF5's last dimension is the fastest changing.
    std::vector<hsize_t> relative_position(ndims), reset(ndims);

    while (!iter.finished()) {
        hsize_t block_size = iter.current_block_size();
        buffer.resize(block_size);

        const auto& starts = iter.starts();
        const auto& counts = iter.counts();
        std::fill(relative_position.begin(), relative_position.end(), 0);

        size_t index = 0;
        for (size_t d = 0; d < ndims; ++d) {
            index += jumps[d] * starts[d];
            reset[d] += jumps[d] * counts[d];
        }

        for (hsize_t i = 0; i < block_size; ++i) {
            buffer[i].offset = cumulative[index];
            buffer[i].length = lengths[index];

            for (hsize_t d1 = ndims; d1 > 0; --d1) { // HDF5's last dimension is the fastest changing.
                hsize_t d = d1 - 1;
                auto& relpos = relative_position[d];
                ++relpos;
                if (relpos < counts[d]) {
                    index += jumps[d];
                    break;
                } 
                relpos = 0;
                index -= reset[d];
            }
        }

        // Scope this to ensure that 'mspace' doesn't get changed by
        // 'iter.next()' before the destructor is called.
        {
            const auto& mspace = iter.memory_space();
            handle.write(buffer.data(), dtype, mspace, iter.file_space());
        }
        iter.next();
    }

    return R_NilValue;
}

//[[Rcpp::export(rng=false)]]
Rcpp::CharacterVector parse_vls_pointers(SEXP pid, SEXP hid, size_t buffer_size) {
    H5::DataSet phandle(STRSXP_2_HID(pid));
    auto pspace = phandle.getSpace();

    hsize_t ndims = pspace.getSimpleExtentNdims();
    std::vector<hsize_t> dims(ndims); 
    pspace.getSimpleExtentDims(dims.data());
    auto block = ritsuko::hdf5::pick_nd_block_dimensions(phandle.getCreatePlist(), dims, buffer_size);

    ritsuko::hdf5::IterateNdDataset iter(dims, block);
    std::vector<ritsuko::hdf5::vls::Pointer<uint64_t, uint64_t> > buffer;
    auto dtype = ritsuko::hdf5::vls::define_pointer_datatype<uint64_t, uint64_t>();

    hsize_t full_length = 1;
    for (auto d : dims) {
        full_length *= d;
    }
    Rcpp::CharacterVector output(full_length);

    std::vector<hsize_t> jumps(ndims, 1);
    hsize_t jump = 1;
    for (size_t d = 0; d < ndims; ++d) {
        jumps[d] = jump;
        jump *= dims[d];
    }
    std::reverse(jumps.begin(), jumps.end()); // reversing as HDF5's last dimension is the fastest changing.
    std::vector<hsize_t> relative_position(ndims), reset(ndims);

    H5::DataSet hhandle(STRSXP_2_HID(hid));
    auto h_dspace = hhandle.getSpace();
    H5::DataSpace h_mspace;
    hsize_t heap_full_length = ritsuko::hdf5::get_1d_length(h_dspace, false);
    std::string strbuffer;
    std::vector<uint8_t> heapbuffer;

    while (!iter.finished()) {
        hsize_t block_size = iter.current_block_size();
        buffer.resize(block_size);

        // Scope this to ensure that 'mspace' doesn't get changed by
        // 'iter.next()' before the destructor is called.
        {
            const auto& mspace = iter.memory_space();
            phandle.read(buffer.data(), dtype, mspace, iter.file_space());
        }

        const auto& starts = iter.starts();
        const auto& counts = iter.counts();
        std::fill(relative_position.begin(), relative_position.end(), 0);

        size_t index = 0;
        for (size_t d = 0; d < ndims; ++d) {
            index += jumps[d] * starts[d];
            reset[d] += jumps[d] * counts[d];
        }

        for (hsize_t i = 0; i < block_size; ++i) {
            hsize_t start = buffer[i].offset;
            hsize_t count = buffer[i].length;
            if (start > heap_full_length || start + count > heap_full_length) {
                throw std::runtime_error("VLS array pointers are out of range of the heap");
            }

            strbuffer.clear();
            if (count) {
                h_mspace.setExtentSimple(1, &count);
                h_mspace.selectAll();
                h_dspace.selectHyperslab(H5S_SELECT_SET, &count, &start);
                heapbuffer.resize(count);
                hhandle.read(heapbuffer.data(), H5::PredType::NATIVE_UINT8, h_mspace, h_dspace);
                const char* text_ptr = reinterpret_cast<const char*>(heapbuffer.data());
                strbuffer.insert(strbuffer.end(), text_ptr, text_ptr + ritsuko::hdf5::find_string_length(text_ptr, count));
            }
            output[index] = strbuffer;

            for (hsize_t d1 = ndims; d1 > 0; --d1) { // HDF5's last dimension is the fastest changing.
                hsize_t d = d1 - 1;
                auto& relpos = relative_position[d];
                ++relpos;
                if (relpos < counts[d]) {
                    index += jumps[d];
                    break;
                } 
                relpos = 0;
                index -= reset[d];
            }
        }

        iter.next();
    }

    return R_NilValue;
}
