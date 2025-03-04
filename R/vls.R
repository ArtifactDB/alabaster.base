#' @export
h5_use_vls <- function(len) {
    max(len) * length(len) >= 16 * length(len) + sum(len)
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
h5_read_vls_array <- function(file, pointers, heap, buffer.size=1e6, keep.dim=TRUE, native=FALSE) {
    parse_vls(
        file,
        pointers,
        heap,
        buffer.size,
        keep.dim,
        native
    )
}
