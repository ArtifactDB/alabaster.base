#' VLS saving utilities
#'
#' Utilities for saving our custom variable length string array format in HDF5.
#' Intended for \pkg{alabaster.*} developers only.
#'
#' @name vls
#' @aliases 
#' h5_use_vls
#' h5_write_vls_array
#' h5_parse_vls_array
NULL

#' @export
h5_use_vls <- function(x) {
    use_vls(x)
}

#' @export
h5_write_vls_array <- function(file, group, pointers, heap, x, compress=6, chunks=NULL, buffer.size=1e6, scalar=FALSE) {
    d <- dim(x)
    if (is.null(d)) {
        d <- length(x)
        if (is.null(chunks)) {
            chunks <- h5_guess_vector_chunks(d)
        }
    } else {
        if (is.null(chunks)) {
            stop("'chunks=' must be specified for high-dimensional 'x'")
        }
    }

    dump_vls(
        file,
        group,
        pointers,
        heap,
        x,
        raw_dims=d,
        raw_chunks=chunks,
        compress=compress,
        scalar=scalar,
        buffer_size=buffer.size
    )
}

#' @export
h5_read_vls_array <- function(file, pointers, heap, missing.placeholder=NULL, buffer.size=1e6, native=FALSE) {
    parse_vls(
        file,
        pointers,
        heap,
        buffer_size=buffer.size,
        placeholder=missing.placeholder,
        native=native
    )
}
