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
    on.exit(H5Fclose(fhandle))

    host <- "atomic_vector"
    ghandle <- H5Gopen(fhandle, host)
    on.exit(H5Gclose(ghandle))
    attrs <- h5readAttributes(fhandle, host)

    full.name <- paste0(host, "/values")
    vattrs <- h5readAttributes(fhandle, full.name)
    contents <- as.vector(h5read(fhandle, full.name))
    contents <- .h5cast(contents, attrs=vattrs, type=attrs$type)

    if (attrs$type == "string") {
        if (!is.null(attrs$format)) {
            if (attrs$format == "date") {
                contents <- as.Date(contents)
            } else if (attrs$format == "date-time") {
                contents <- .cast_datetime(contents)
            }
        }
    }

    if ("names" %in% h5ls(ghandle, recursive=FALSE, datasetinfo=FALSE)$name) {
        names(contents) <- as.vector(h5read(ghandle, "names"))
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
