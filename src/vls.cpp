#include "Rcpp.h"

#include "ritsuko/hdf5/vls/vls.hpp"
#include "H5Cpp.h"

H5::DSetCreatPropList initialize_creat_prop_list(const std::vector<hsize_t>& chunks, int compress, bool scalar) {
    if (compress == 0) {
        return H5::DSetCreatPropList::DEFAULT;
    }
    H5::DSetCreatPropList cplist;
    H5Pset_obj_track_times(cplist.getId(), 0); // hm, don't know the C++ version of this.
    cplist.setFillTime(H5D_FILL_TIME_NEVER);
    cplist.setShuffle();
    cplist.setDeflate(compress);
    cplist.setChunk(chunks.size(), chunks.data());
    return cplist;
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

    // Firstly saving the pointers.
    {
        std::vector<hsize_t> chunks;
        if (raw_chunks.isNotNull()) {
            Rcpp::IntegerVector tmp(raw_chunks);
            chunks.insert(chunks.end(), tmp.begin(), tmp.end());
        }
        auto pcplist = initialize_creat_prop_list(chunks, compress, scalar);

        size_t ndims = raw_dims.size();
        std::vector<hsize_t> dims(raw_dims.begin(), raw_dims.end()); 
        H5::DataSpace pdspace(ndims, dims.data()); 
        auto pdtype = ritsuko::hdf5::vls::define_pointer_datatype<uint64_t, uint64_t>();
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
                buffer[i].length = cumulative[index + 1] - cumulative[index];

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
        auto hcplist = initialize_creat_prop_list(chunks, true, false);

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
    bool keep_dim,
    bool native)
{
    H5::H5File handle(file, H5F_ACC_RDWR);
    H5::DataSet phandle = handle.openDataSet(pointers);
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

    auto hhandle = handle.openDataSet(heap);
    auto hdspace = hhandle.getSpace();
    H5::DataSpace hmspace;
    hsize_t hfull_length = ritsuko::hdf5::get_1d_length(hdspace, false);
    std::vector<uint8_t> hbbuffer;
    std::string hsbuffer;

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
            output[index] = hsbuffer;

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

    return output;
}
