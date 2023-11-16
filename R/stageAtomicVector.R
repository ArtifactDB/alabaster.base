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
#' @name saveAtomicVector
#' @rdname stageAtomicVector
#' @aliases
#' stageObject,integer-method
#' stageObject,numeric-method
#' stageObject,logical-method
#' stageObject,character-method
#' stageObject,double-method
#' stageObject,POSIXct-method
#' stageObject,Date-method
NULL

#' @importFrom S4Vectors DataFrame
#' @importFrom rhdf5 h5createFile h5createGroup H5Fopen H5Fclose H5Gopen H5Gclose h5writeAttribute
.save_atomic_vector <- function(x, dir, path, ...) {
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

    ofile <- file.path(dir, path, "contents.h5")
    h5createFile(ofile)
    host <- "atomic_vector"
    h5createGroup(ofile, host)

    fhandle <- H5Fopen(ofile)
    on.exit(H5Fclose(fhandle), add=TRUE)
    (function (){
        ghandle <- H5Gopen(fhandle, host)
        on.exit(H5Gclose(ghandle), add=TRUE)
        h5writeAttribute("1.0", ghandle, "version", asScalar=TRUE)
    })()

    transformed <- transformVectorForHdf5(contents)
    current <- transformed$transformed
    missing.placeholder <- transformed$placeholder

    full.data.name <- paste0(host, "/values")
    h5write(current, fhandle, full.data.name)
    if (!is.null(missing.placeholder)) {
        addMissingPlaceholderAttributeForHdf5(fhandle, full.data.name, missing.placeholder)
    }

    (function() {
        dhandle <- H5Dopen(fhandle, full.data.name)
        on.exit(H5Dclose(dhandle), add=TRUE)
        h5writeAttribute(type, dhandle, "type", asScalar=TRUE)
        if (!is.null(format)) {
            h5writeAttribute(colmeta$format, dhandle, "format", asScalar=TRUE)
        }
    })()

    if (!is.null(names(x))) {
        h5write(names(x), fhandle, paste0(host, "/names"))
    }
    full.data.name <- paste0(host, "/values")

    write("atomic_vector", file=file.path(dir, path, "OBJECT"))
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
