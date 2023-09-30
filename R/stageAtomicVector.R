#' @importFrom S4Vectors DataFrame
.stage_atomic_vector <- function(x, dir, path, child=FALSE, ...) {
    dir.create(file.path(dir, path), showWarnings=FALSE)
    new_path <- paste0(path, "/simple.txt.gz")

    if (.is_datetime(x)) {
        type <- "string"
        format <- "date-time"
        contents <- .sanitize_datetime(x)

    } else if (is(x, "Date")) {
        type <- "string"
        format <- "date"
        contents <- .sanitize_date(x)

    } else {
        stopifnot(is.atomic(x))
        remapped <- .remap_atomic_type(x)
        type <- remapped$type
        contents <- remapped$values
        format <- NULL
    }

    mock <- DataFrame(values=contents)
    if (!is.null(names(x))) {
        mock <- cbind(names=names(x), mock)
    }
    quickWriteCsv(mock, file.path(dir, new_path), row.names=FALSE, compression="gzip")

    list(
        `$schema` = "atomic_vector/v1.json",
        path = new_path,
        is_child = child,
        atomic_vector = list(
            type = type,
            length = length(contents),
            names = !is.null(names(x)),
            format = format,
            compression="gzip"
        )
    )
}

#' Stage atomic vectors
#'
#' Stage vectors containing atomic elements (or values that can be cast as such, e.g., dates and times).
#'
#' @param x Any of the atomic vector types, or \link{Date} objects, or time objects, e.g., \link{POSIXct}.
#' @inheritParams stageObject
#' @param ... Further arguments that are ignored.
#' 
#' @return
#' A named list containing the metadata for \code{x}.
#' \code{x} itself is written to a CSV file inside \code{path}.
#'
#' @details 
#' Dates and POSIX times are cast to strings;
#' the type itself is recorded in the metadata.
#'
#' @author Aaron Lun
#' 
#' @examples
#' tmp <- tempfile()
#' dir.create(tmp)
#' stageObject(LETTERS, tmp, path="foo")
#' stageObject(setNames(runif(26), letters), tmp, path="bar")
#'
#' list.files(tmp, recursive=TRUE)
#' 
#' @name stageAtomicVector
NULL

#' @export
#' @rdname stageAtomicVector
setMethod("stageObject", "integer", .stage_atomic_vector)

#' @export
#' @rdname stageAtomicVector
setMethod("stageObject", "character", .stage_atomic_vector)

#' @export
#' @rdname stageAtomicVector
setMethod("stageObject", "logical", .stage_atomic_vector)

#' @export
#' @rdname stageAtomicVector
setMethod("stageObject", "double", .stage_atomic_vector)

#' @export
#' @rdname stageAtomicVector
setMethod("stageObject", "numeric", .stage_atomic_vector)

#' @export
#' @rdname stageAtomicVector
setMethod("stageObject", "Date", .stage_atomic_vector)

#' @export
#' @rdname stageAtomicVector
setMethod("stageObject", "POSIXlt", .stage_atomic_vector)

#' @export
#' @rdname stageAtomicVector
setMethod("stageObject", "POSIXct", .stage_atomic_vector)
