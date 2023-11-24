#' HDF5 utilities
#'
#' Basically just better versions of those in \pkg{rhdf5}, dedicated to \pkg{alabaster.base} and its dependents.
#' Intended for \pkg{alabaster.*} developers only.
#'
#' @name hdf5
#' @import rhdf5
#' @aliases 
#' h5_exists
#' h5_write_vector
#' h5_write_attribute
NULL

#' @export
h5_exists <- function(handle, name) {
    name %in% h5ls(handle, datasetinfo=FALSE, recursive=FALSE)$name
}

.choose_type <- function(x, plus.one=FALSE) {
    if (is.integer(x)) {
        type <- "H5T_NATIVE_INT32"
    } else if (is.double(x)) {
        type <- "H5T_NATIVE_DOUBLE"
    } else if (is.character(x)) {
        tid <- H5Tcopy("H5T_C_S1")
        H5Tset_strpad(tid, strpad = "NULLPAD")
        H5Tset_size(tid, max(nchar(x, type="bytes") + plus.one, 1L))

        enc <- unique(Encoding(x))
        if ("UTF-8" %in% enc) {
            H5Tset_cset(tid, "UTF8")
        } else {
            H5Tset_cset(tid, "ASCII")
        }

        tid
    } else {
        stop("unsupported type '", typeof(x) + "'")
    }
}

#' @export
h5_write_vector <- function(handle, name, x, type=NULL, compress=6, chunks=NULL, scalar=FALSE, emit=FALSE) {
    if (is.null(type)) {
        type <- .choose_type(x)
    }

    if (length(x) == 1 && scalar) {
        shandle <- H5Screate("H5S_SCALAR")
    } else {
        shandle <- H5Screate_simple(length(x))
    }
    on.exit(H5Sclose(shandle), add=TRUE, after=FALSE)

    phandle <- H5Pcreate("H5P_DATASET_CREATE")
    on.exit(H5Pclose(phandle), add=TRUE, after=FALSE)
    H5Pset_fill_time(phandle, "H5D_FILL_TIME_ALLOC")
    if (compress > 0) {
        H5Pset_deflate(phandle, level=compress)
        if (is.null(chunks)) { # Some reasonable number between 10000 and 100000, depending on the size. 
            chunks <- sqrt(length(x))
            if (chunks < 10000) {
                chunks <- min(length(x), 10000)
            } else if (chunks > 100000) {
                chunks <- 100000
            }
        }
        H5Pset_chunk(phandle, chunks)
    }

    dhandle <- H5Dcreate(handle, name, dtype_id=type, h5space=shandle, dcpl=phandle)
    if (!emit) {
        on.exit(H5Dclose(dhandle), add=TRUE, after=FALSE)
    }
    H5Dwrite(dhandle, x)

    if (emit) {
        return(dhandle)
    } else {
        return(invisible(NULL))
    }
}

#' @export
h5_write_attribute <- function(handle, name, x, type=NULL, scalar=FALSE) {
    if (is.null(type)) {
        type <- .choose_type(x, plus.one=TRUE) # who knows why H5Awrite forces zero-termination?
    }

    if (length(x) == 1 && scalar) {
        shandle <- H5Screate("H5S_SCALAR")
    } else {
        shandle <- H5Screate_simple(length(x))
    }
    on.exit(H5Sclose(shandle), add=TRUE, after=FALSE)

    ahandle <- H5Acreate(handle, name, dtype_id=type, h5space=shandle)
    on.exit(H5Aclose(ahandle), add=TRUE, after=FALSE)
    H5Awrite(ahandle, x)

    invisible(NULL)
}
