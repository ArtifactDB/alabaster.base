#' HDF5 utilities
#'
#' Basically just better versions of those in \pkg{rhdf5}, dedicated to \pkg{alabaster.base} and its dependents.
#' Intended for \pkg{alabaster.*} developers only.
#'
#' @name hdf5
#' @import rhdf5
#' @aliases 
#' h5_guess_vector_chunks 
#' h5_create_vector
#' h5_write_vector
#' h5_write_attribute
#' h5_read_vector
#' h5_read_attribute
#' h5_object_exists
#' h5_cast
NULL

.choose_type <- function(x) {
    if (is.integer(x)) {
        type <- "H5T_NATIVE_INT32"
    } else if (is.double(x)) {
        type <- "H5T_NATIVE_DOUBLE"
    } else if (is.character(x)) {
        tid <- H5Tcopy("H5T_C_S1")
        H5Tset_strpad(tid, strpad = "NULLPAD")
        H5Tset_size(tid, max(nchar(x, type="bytes"), 1L, na.rm=TRUE))

        enc <- unique(Encoding(x))
        if ("UTF-8" %in% enc) {
            H5Tset_cset(tid, "UTF8")
        } else {
            H5Tset_cset(tid, "ASCII")
        }

        tid
    } else {
        stop("unsupported type '", typeof(x), "'")
    }
}

#' @export
h5_guess_vector_chunks <- function(len) {
    # Some reasonable number between 10000 and 100000, depending on the size. 
    chunks <- sqrt(len)
    if (chunks < 10000) {
        chunks <- min(len, 10000)
    } else if (chunks > 100000) {
        chunks <- 100000
    }
    return(chunks)
}

#' @export
h5_create_vector <- function(handle, name, len, type, compress=6, chunks=NULL, scalar=FALSE, emit=FALSE) {
    if (len == 1 && scalar) {
        shandle <- H5Screate("H5S_SCALAR")
        compress <- 0
    } else {
        shandle <- H5Screate_simple(len)
    }
    on.exit(H5Sclose(shandle), add=TRUE, after=FALSE)

    phandle <- H5Pcreate("H5P_DATASET_CREATE")
    on.exit(H5Pclose(phandle), add=TRUE, after=FALSE)
    H5Pset_fill_time(phandle, "H5D_FILL_TIME_NEVER")
    H5Pset_obj_track_times(phandle, FALSE)

    if (compress > 0 && len) {
        H5Pset_shuffle(phandle)
        H5Pset_deflate(phandle, level=compress)
        if (is.null(chunks)) {
            chunks <- h5_guess_vector_chunks(len)
        }
        H5Pset_chunk(phandle, chunks)
    }

    H5Dcreate(handle, name, dtype_id=type, h5space=shandle, dcpl=phandle)
}

#' @export
h5_write_vector <- function(handle, name, x, type=NULL, compress=6, chunks=NULL, scalar=FALSE, emit=FALSE) {
    if (is.null(type)) {
        if (is.character(x)) {
            x <- enc2utf8(x) # avoid mis-encoding multi-byte characters from Latin-1.
        }
        type <- .choose_type(x)
    }

    dhandle <- h5_create_vector(handle, name, length(x), type=type, compress=compress, chunks=chunks, scalar=scalar, emit=emit)
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
        if (is.character(x)) {
            x <- enc2utf8(x) # avoid mis-encoding multi-byte characters from Latin-1.
        }
        type <- .choose_type(x)
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

#' @export
h5_object_exists <- function(handle, name) {
    name %in% h5ls(handle, datasetinfo=FALSE, recursive=FALSE)$name
}

#' @export
h5_read_vector <- function(handle, name, check=FALSE, default=NULL, bit64conversion) {
    if (check) {
        if (!h5_object_exists(handle, name)) {
            return(default)
        }
    }

    dhandle <- H5Dopen(handle, name)
    on.exit(H5Dclose(dhandle), add=TRUE, after=FALSE)
    output <- H5Dread(dhandle, bit64conversion=bit64conversion, drop=TRUE)

    if (is.raw(output)) {
        storage.mode(output) <- "integer"
    } else if (is.character(output) && H5Tget_cset(H5Dget_type(dhandle)) == 1L) {
        Encoding(output) <- "UTF-8"
    }

    output
}

#' @export
h5_read_attribute <- function(handle, name, check=FALSE, default=NULL, bit64conversion) {
    if (check) {
        if (!H5Aexists(handle, name)) {
            return(default)
        }
    }

    ahandle <- H5Aopen_by_name(handle, name=name)
    on.exit(H5Aclose(ahandle), add=TRUE, after=FALSE)
    output <- H5Aread(ahandle, bit64conversion=bit64conversion)

    if (is.raw(output)) {
        storage.mode(output) <- "integer"
    } else if (is.character(output) && H5Tget_cset(H5Aget_type(ahandle)) == 1L) {
        Encoding(output) <- "UTF-8"
    }

    output
}

#' @export
h5_cast <- function(current, expected.type, missing.placeholder, respect.nan.payload=FALSE) {
    restore_min_integer <- function(y) {
        z <- FALSE
        if (is.integer(y) && anyNA(y)) { # promote integer NAs back to the actual number.
            y <- as.double(y)
            y[is.na(y)] <- -2^31
            z <- TRUE
        }
        list(y=y, converted=z)
    }

    converted <- FALSE
    if (is.null(missing.placeholder)) {
        out <- restore_min_integer(current)
        current <- out$y
        converted <- out$converted
    } else if (is.na(missing.placeholder)) {
        if (!is.double(current)) {
            # No-op as the placeholder is already R's NA of the relevant type.
        } else {
            if (respect.nan.payload && !is.nan(missing.placeholder)) {
                # No-op as we're considering the NaN payload and the placeholder
                # is not NaN (and thus must be an R NA).
            } else { 
                # We set all NaN values to missing, regardless of their payloads.
                # This is possible because is.na(), despite its name, calls the
                # C isnan() under the hood, which just returns TRUE for all NaNs.
                current[is.na(current)] <- NA 
            }
        }
    } else {
        out <- restore_min_integer(current)
        current <- out$y
        converted <- out$converted
        if (is.raw(current)) {
            current <- as.integer(current) # doing this here as raw vectors cannot contain NAs.
        }
        current[which(current == missing.placeholder)] <- NA # Using which() to avoid problems with existing NAs.
    }

    if (!converted && !is.null(expected.type)) {
        target.type <- as.character(.atomics[expected.type])
        if (target.type != typeof(current)) {
            storage.mode(current) <- target.type
        }
    }

    current
}
