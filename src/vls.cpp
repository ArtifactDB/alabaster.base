#include "Rcpp.h"

#include "ritsuko/hdf5/vls/vls.hpp"
#include "H5Cpp.h"

#include <algorithm>
#include <string>
#include <vector>
#include <cstdint>

//[[Rcpp::export(rng=false)]]
bool use_vls(Rcpp::CharacterVector x) {
    size_t max = 0, sum = 0;
    for (Rcpp::String val : x) {
        size_t l = Rf_length(val.get_sexp());
        sum += l;
        max = std::max(max, l);
    }
    // Checking whether the pointer overhead is worth it.
    return max * x.size() > sum + 2 * sizeof(uint64_t) * x.size();
}

H5::DSetCreatPropList initialize_creat_prop_list(const std::vector<hsize_t>& chunks, int compress) {
    if (compress == 0) {
        return H5::DSetCreatPropList::DEFAULT;
    } else {
        H5::DSetCreatPropList cplist;
        H5Pset_obj_track_times(cplist.getId(), 0); // hm, don't know the C++ version of this.
        cplist.setFillTime(H5D_FILL_TIME_NEVER);
        cplist.setShuffle();
        cplist.setDeflate(compress);
        cplist.setChunk(chunks.size(), chunks.data());
        return cplist;
    }
}

//[[Rcpp::export(rng=false)]]
SEXP dump_vls(
    std::string file,
    std::string group,
    std::string pointers,
    std::string heap,
    Rcpp::CharacterVector x,
    Rcpp::IntegerVector raw_dims,
    Rcpp::Nullable<Rcpp::IntegerVector> raw_chunks,
    int compress,
    bool scalar,
    size_t buffer_size)
{
    H5::H5File handle(file, H5F_ACC_RDWR);
    auto ghandle = handle.openGroup(group);
    size_t heap_length = 0;
    auto pdtype = ritsuko::hdf5::vls::define_pointer_datatype<uint64_t, uint64_t>();

    // Firstly saving the pointers.
    if (scalar) {
        if (x.size() != 1) {
            throw std::runtime_error("expected 'x' to have length 1 for 'scalar=true'");
        }
        Rcpp::String val(x[0]);

        ritsuko::hdf5::vls::Pointer<uint64_t, uint64_t> pbuffer;
        pbuffer.offset = 0;
        pbuffer.length = Rf_length(val.get_sexp());
        heap_length = pbuffer.length;

        H5::DSetCreatPropList pcplist(H5::DSetCreatPropList::DEFAULT);
        auto phandle = ghandle.createDataSet(pointers, pdtype, H5S_SCALAR, pcplist);
        phandle.write(&pbuffer, pdtype);

    } else {
        std::vector<hsize_t> chunks;
        if (raw_chunks.isNotNull()) {
            Rcpp::IntegerVector tmp(raw_chunks);
            chunks.insert(chunks.end(), tmp.begin(), tmp.end());
            std::reverse(chunks.begin(), chunks.end()); // saving in native form, see comments below.
        }
        auto pcplist = initialize_creat_prop_list(chunks, compress);

        size_t ndims = raw_dims.size();
        std::vector<hsize_t> dims(raw_dims.begin(), raw_dims.end()); 
        std::reverse(dims.begin(), dims.end()); // saving in native form, so we need to transpose to match the real fastest changing dimension.
        H5::DataSpace pdspace(ndims, dims.data()); 
        auto phandle = ghandle.createDataSet(pointers, pdtype, pdspace, pcplist);

        auto block = ritsuko::hdf5::pick_nd_block_dimensions(pcplist, dims, buffer_size);
        ritsuko::hdf5::IterateNdDataset iter(dims, block);
        std::vector<ritsuko::hdf5::vls::Pointer<uint64_t, uint64_t> > buffer;

        std::vector<uint64_t> cumulative(1);
        cumulative.reserve(x.size() + 1);
        for (Rcpp::String val : x) {
            size_t l = Rf_length(val.get_sexp());
            heap_length += l;
            cumulative.push_back(heap_length);
        }

        std::vector<hsize_t> jumps(ndims, 1);
        hsize_t jump = 1;
        for (size_t d1 = ndims; d1 > 0; --d1) { // reversing as HDF5's last dimension is the fastest changing.
            size_t d = d1 - 1; 
            jumps[d] = jump;
            jump *= dims[d];
        }
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
                reset[d] = jumps[d] * counts[d];
            }

            for (hsize_t i = 0; i < block_size; ++i) {
                buffer[i].offset = cumulative[index];
                buffer[i].length = cumulative[index + 1] - cumulative[index];

                for (hsize_t d1 = ndims; d1 > 0; --d1) { // again, HDF5's last dimension is the fastest changing, so we go back to front.
                    hsize_t d = d1 - 1;
                    auto& relpos = relative_position[d];
                    ++relpos;
                    index += jumps[d];
                    if (relpos < counts[d]) {
                        break;
                    } 
                    relpos = 0;
                    index -= reset[d];
                }
            }

            phandle.write(buffer.data(), pdtype, iter.memory_space(), iter.file_space());
            iter.next();
        }
    }

    // Now saving the heap.
    {
        std::vector<hsize_t> chunks;
        chunks.push_back(std::ceil(std::sqrt(static_cast<double>(heap_length))));
        if (chunks.front() < 1000) {
            chunks[0] = std::min(static_cast<size_t>(1000), heap_length);
        } else if (chunks.front() > 50000) {
            chunks[0] = 50000;
        }
        auto hcplist = initialize_creat_prop_list(chunks, compress);

        hsize_t hdims = heap_length;
        H5::DataSpace hspace(1, &hdims), mspace;
        auto hhandle = ghandle.createDataSet(heap, H5::PredType::NATIVE_UINT8, hspace, hcplist);

        std::vector<uint8_t> buffer;
        size_t vlen = x.size();
        hsize_t last = 0;
        auto save_buffer = [&]() -> void {
            hsize_t bsize = buffer.size();
            mspace.setExtentSimple(1, &bsize);
            mspace.selectAll();
            hspace.selectHyperslab(H5S_SELECT_SET, &bsize, &last);
            hhandle.write(buffer.data(), H5::PredType::NATIVE_UINT8, mspace, hspace);
        };

        for (size_t i = 0; i < vlen; ++i) {
            Rcpp::String val(x[i]);
            size_t len = Rf_length(val.get_sexp());
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
    }

    return R_NilValue;
}

//[[Rcpp::export(rng=false)]]
Rcpp::CharacterVector parse_vls(
    std::string file,
    std::string pointers,
    std::string heap,
    size_t buffer_size,
    Rcpp::Nullable<Rcpp::CharacterVector> placeholder,
    bool native)
{
    H5::H5File handle(file, H5F_ACC_RDWR);
    auto phandle = handle.openDataSet(pointers);
    auto pspace = phandle.getSpace();

    auto hhandle = handle.openDataSet(heap);
    auto hdspace = hhandle.getSpace();
    hsize_t hfull_length = ritsuko::hdf5::get_1d_length(hdspace, false);
    H5::DataSpace hmspace;

    hsize_t ndims = pspace.getSimpleExtentNdims();
    auto dtype = ritsuko::hdf5::vls::define_pointer_datatype<uint64_t, uint64_t>();
    std::vector<uint8_t> hbbuffer;
    std::string hsbuffer;

    bool has_placeholder = placeholder.isNotNull();
    std::string placeholder_value;
    if (has_placeholder) {
        Rcpp::CharacterVector raw_placeholder_array(placeholder);
        if (raw_placeholder_array.size() != 1) {
            throw std::runtime_error("expected 'placeholder' to be a character vector of length 1");
        }
        Rcpp::String placeholder_string(raw_placeholder_array[0]);
        if (placeholder_string == NA_STRING) {
            throw std::runtime_error("expected 'placeholder' to be a non-NA string");
        }
        placeholder_value = placeholder_string;
    }

    auto fill_heap_buffers = [&](hsize_t start, hsize_t count, Rcpp::CharacterVector& output, size_t index) -> void { 
        if (start > hfull_length || start + count > hfull_length) {
            throw std::runtime_error("VLS array pointers are out of range of the heap");
        }

        hsbuffer.clear();
        if (count) {
            hmspace.setExtentSimple(1, &count);
            hmspace.selectAll();
            hdspace.selectHyperslab(H5S_SELECT_SET, &count, &start);
            hbbuffer.resize(count);
            hhandle.read(hbbuffer.data(), H5::PredType::NATIVE_UINT8, hmspace, hdspace);
            const char* text_ptr = reinterpret_cast<const char*>(hbbuffer.data());
            hsbuffer.insert(hsbuffer.end(), text_ptr, text_ptr + ritsuko::hdf5::find_string_length(text_ptr, count));
        }

        if (has_placeholder && hsbuffer == placeholder_value) {
            output[index] = NA_STRING;
        } else {
            output[index] = hsbuffer;
        }
    };

    if (ndims == 0) {
        ritsuko::hdf5::vls::Pointer<uint64_t, uint64_t> pbuffer;
        hhandle.read(&pbuffer, dtype);
        Rcpp::CharacterVector output(1);
        fill_heap_buffers(pbuffer.offset, pbuffer.length, output, 0);
        return output;
    }

    std::vector<hsize_t> dims(ndims); 
    pspace.getSimpleExtentDims(dims.data());
    auto block = ritsuko::hdf5::pick_nd_block_dimensions(phandle.getCreatePlist(), dims, buffer_size);
    ritsuko::hdf5::IterateNdDataset iter(dims, block);

    std::vector<hsize_t> jumps(ndims, 1);
    hsize_t prod = 1;
    if (native) {
        for (size_t d = 0; d < ndims; ++d) {
            jumps[d] = prod;
            prod *= dims[d];
        }
    } else {
        for (size_t d1 = ndims; d1 > 0; --d1) {
            size_t d = d1 - 1;
            jumps[d] = prod;
            prod *= dims[d];
        }
    }
    std::vector<hsize_t> relative_position(ndims), reset(ndims);
    Rcpp::CharacterVector output(prod);
    std::vector<ritsuko::hdf5::vls::Pointer<uint64_t, uint64_t> > buffer;

    while (!iter.finished()) {
        hsize_t block_size = iter.current_block_size();
        buffer.resize(block_size);
        phandle.read(buffer.data(), dtype, iter.memory_space(), iter.file_space());

        const auto& starts = iter.starts();
        const auto& counts = iter.counts();
        std::fill(relative_position.begin(), relative_position.end(), 0);

        size_t index = 0;
        for (size_t d = 0; d < ndims; ++d) {
            index += jumps[d] * starts[d];
            reset[d] = jumps[d] * counts[d];
        }

        for (hsize_t i = 0; i < block_size; ++i) {
            fill_heap_buffers(buffer[i].offset, buffer[i].length, output, index);
            for (size_t d1 = ndims; d1 > 0; --d1) { // HDF5's last dimension is the fastest changing.
                size_t d = d1 - 1;
                auto& relpos = relative_position[d];
                ++relpos;
                index += jumps[d];
                if (relpos < counts[d]) {
                    break;
                } 
                relpos = 0;
                index -= reset[d];
            }
        }

        iter.next();
    }

    Rcpp::IntegerVector dim_attr(dims.begin(), dims.end());
    if (!native) {
        std::reverse(dim_attr.begin(), dim_attr.end());
    }
    output.attr("dim") = dim_attr;
    return output;
}
