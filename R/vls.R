#' @export
h5_use_vls <- function(len) {
    max(len) * length(len) >= 16 * length(len) + sum(len)
}

#' @export
h5_create_vls_pointer_dataset <- function(handle, name, dimensions, chunks, compress=6, scalar=FALSE, emit=TRUE) {
    create_vls_pointer_dataset(handle, name, dimensions, chunks, compress, scalar)
    if (emit) {
        return(H5Dopen(handle, name))
    }
}

#' @export
h5_write_vls_array <- function(pointers, heap, x, len, buffer.size=1e6) {
    dump_vls_pointers(pointers, len, buffer.size)
    dump_vls_heap(heap, x, len, buffer.size)
}

#' @export
#' @importFrom rhdf5 H5Dget_space H5Sclose H5Sget_simple_extent_dims
h5_read_vls_array <- function(pointers, heap, buffer.size=1e6, keep.dim=TRUE, transposed=TRUE) {
    parse_vls_pointers(pointers, heap, buffer.size)
}
