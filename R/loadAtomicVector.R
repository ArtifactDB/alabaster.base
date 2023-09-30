#' Load an atomic vector
#'
#' Load a simple vector consisting of atomic elements from file.
#'
#' @inheritParams loadDataFrame
#' @param ... Further arguments, ignored.
#'
#' @return The vector described by \code{info}, possibly with names.
#'
#' @seealso
#' \code{"\link{stageObject,integer-method}"}, for one of the staging methods.
#'
#' @author Aaron Lun
#' 
#' @examples
#' tmp <- tempfile()
#' dir.create(tmp)
#' meta <- stageObject(setNames(runif(26), letters), tmp, path="bar")
#' loadAtomicVector(meta, tmp)
#' 
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
