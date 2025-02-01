#' Save atomic vectors to disk
#'
#' Save vectors containing atomic elements (or values that can be cast as such, e.g., dates and times) to an on-disk representation.
#'
#' @param x Any of the atomic vector types, or \link{Date} objects, or time objects, e.g., \link{POSIXct}.
#' @inheritParams saveObject
#' @param ... Further arguments that are ignored.
#' 
#' @return
#' \code{x} is saved inside \code{path}.
#' \code{NULL} is invisibly returned.
#'
#' @author Aaron Lun
#' 
#' @seealso
#' \code{\link{readAtomicVector}}, to read the files back into the session.
#'
#' @examples
#' tmp <- tempfile()
#' dir.create(tmp)
#' saveObject(LETTERS, file.path(tmp, "foo"))
#' saveObject(setNames(runif(26), letters), file.path(tmp, "bar"))
#' list.files(tmp, recursive=TRUE)
#' 
#' @name saveAtomicVector
#' @aliases
#' stageObject,integer-method
#' stageObject,numeric-method
#' stageObject,logical-method
#' stageObject,character-method
#' stageObject,double-method
#' stageObject,POSIXct-method
#' stageObject,POSIXlt-method
#' stageObject,Date-method
NULL

.save_atomic_vector <- function(x, path, ...) {
    dir.create(path)
    ofile <- file.path(path, "contents.h5")

    if (.is_datetime(x)) {
        type <- "string"
        format <- "date-time"
        contents <- .sanitize_datetime(x)

    } else if (is(x, "Date")) {
        type <- "string"
        format <- "date"
        contents <- .sanitize_date(x)

    } else if (is.package_version(x)) {
        type <- "string"
        format <- NULL
        contents <- as.character(x)

    } else {
        stopifnot(is.atomic(x))
        remapped <- .remap_atomic_type(x)
        type <- remapped$type
        contents <- remapped$values
        format <- NULL
    }

    fhandle <- H5Fcreate(ofile, "H5F_ACC_TRUNC")
    on.exit(H5Fclose(fhandle), add=TRUE, after=FALSE)
    ghandle <- H5Gcreate(fhandle, "atomic_vector")
    on.exit(H5Gclose(ghandle), add=TRUE, after=FALSE)

    h5_write_attribute(ghandle, "type", type, scalar=TRUE)
    if (!is.null(format)) {
        h5_write_attribute(ghandle, "format", format, scalar=TRUE)
    }

    transformed <- transformVectorForHdf5(contents)
    current <- transformed$transformed
    missing.placeholder <- transformed$placeholder

    dhandle <- h5_write_vector(ghandle, "values", current, emit=TRUE)
    on.exit(H5Dclose(dhandle), add=TRUE, after=FALSE)
    if (!is.null(missing.placeholder)) {
        h5_write_attribute(dhandle, missingPlaceholderName, missing.placeholder, scalar=TRUE)
    }

    if (!is.null(names(x))) {
        h5_write_vector(ghandle, "names", names(x))
    }

    saveObjectFile(path, "atomic_vector", list(atomic_vector=list(version="1.0")))
    invisible(NULL)
}

#' @export
#' @rdname saveAtomicVector
setMethod("saveObject", "integer", .save_atomic_vector)

#' @export
#' @rdname saveAtomicVector
setMethod("saveObject", "character", .save_atomic_vector)

#' @export
#' @rdname saveAtomicVector
setMethod("saveObject", "logical", .save_atomic_vector)

#' @export
#' @rdname saveAtomicVector
setMethod("saveObject", "double", .save_atomic_vector)

#' @export
#' @rdname saveAtomicVector
setMethod("saveObject", "numeric", .save_atomic_vector)

#' @export
#' @rdname saveAtomicVector
setMethod("saveObject", "Date", .save_atomic_vector)

#' @export
#' @rdname saveAtomicVector
setMethod("saveObject", "POSIXlt", .save_atomic_vector)

#' @export
#' @rdname saveAtomicVector
setMethod("saveObject", "POSIXct", .save_atomic_vector)

#' @export
#' @rdname saveAtomicVector
setMethod("saveObject", "package_version", .save_atomic_vector)

#######################################
########### OLD STUFF HERE ############
#######################################

#' @importFrom S4Vectors DataFrame
.stage_atomic_vector <- function(x, dir, path, child=FALSE, ...) {
    dir.create(file.path(dir, path), showWarnings=FALSE)

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

    new_path <- paste0(path, "/simple.txt.gz")
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


#' @export
setMethod("stageObject", "integer", .stage_atomic_vector)

#' @export
setMethod("stageObject", "character", .stage_atomic_vector)

#' @export
setMethod("stageObject", "logical", .stage_atomic_vector)

#' @export
setMethod("stageObject", "double", .stage_atomic_vector)

#' @export
setMethod("stageObject", "numeric", .stage_atomic_vector)

#' @export
setMethod("stageObject", "Date", .stage_atomic_vector)

#' @export
setMethod("stageObject", "POSIXlt", .stage_atomic_vector)

#' @export
setMethod("stageObject", "POSIXct", .stage_atomic_vector)
