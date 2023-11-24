#' Read an atomic vector from disk
#'
#' Read a vector consisting of atomic elements from its on-disk representation.
#'
#' @param path Path to a directory created with any of the vector methods for \code{\link{saveObject}}.
#' @param ... Further arguments, ignored.
#'
#' @return 
#' The vector described by \code{info}.
#'
#' @seealso
#' \code{"\link{saveObject,integer-method}"}, for one of the staging methods.
#'
#' @author Aaron Lun
#' 
#' @examples
#' tmp <- tempfile()
#' saveObject(setNames(runif(26), letters), tmp)
#' readAtomicVector(tmp)
#' 
#' @export
#' @aliases loadAtomicVector
readAtomicVector <- function(path, ...) {
    fpath <- file.path(path, "contents.h5")
    fhandle <- H5Fopen(fpath)
    on.exit(H5Fclose(fhandle), add=TRUE, after=FALSE)

    host <- "atomic_vector"
    ghandle <- H5Gopen(fhandle, host)
    on.exit(H5Gclose(ghandle), add=TRUE, after=FALSE)
    expected.type <- h5_read_attribute(ghandle, "type")

    vhandle <- H5Dopen(ghandle, "values")
    on.exit(H5Dclose(vhandle), add=TRUE, after=FALSE)
    contents <- H5Dread(vhandle, drop=FALSE)
    missing.placeholder <- h5_read_attribute(vhandle, missing_placeholder_name, check=TRUE, default=NULL)

    contents <- h5_cast(contents, expected.type=expected.type, missing.placeholder=missing.placeholder)
    if (expected.type == "string") {
        if (H5Aexists(ghandle, "format")) {
            format <- h5_read_attribute(ghandle, "format")
            if (format == "date") {
                contents <- as.Date(contents)
            } else if (format == "date-time") {
                contents <- .cast_datetime(contents)
            }
        }
    }

    if (h5_object_exists(ghandle, "names")) {
        names(contents) <- h5_read_vector(ghandle, "names")
    }

    contents
}

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
loadAtomicVector <- function(info, project, ...) {
    fpath <- acquireFile(project, info$path)
    meta <- info$atomic_vector

    df <- read.csv3(fpath, compression=meta$compression, nrows=meta$length)
    output <- df[,ncol(df)]
    type <- meta$type

    if (type == "string") {
        format <- meta$format
        if (!is.null(format)) {
            if (format == "date") {
                output <- as.Date(output)
            } else if (format == "date-time") {
                output <- .cast_datetime(output)
            } else {
                output <- as.character(output)
            }
        }
    } else {
        stopifnot(.is_atomic(type))
        output <- .cast_atomic(output, type)
    }

    if (isTRUE(meta$names)) {
        names(output) <- df[,1]
    }
    output 
}
