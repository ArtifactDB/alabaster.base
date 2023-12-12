#' Save a factor to disk
#'
#' Pretty much as it says, let's save a base R \link{factor} to an on-disk representation.
#'
#' @param x A factor.
#' @inheritParams saveObject
#' @param ... Further arguments that are ignored.
#'
#' @return
#' \code{x} is saved inside \code{path}.
#' \code{NULL} is invisibly returned.
#'
#' @seealso
#' \code{\link{readBaseFactor}}, to read the files back into the session.
#'
#' @author Aaron Lun
#' 
#' @examples
#' tmp <- tempfile()
#' saveObject(factor(1:10, 1:30), tmp)
#' list.files(tmp, recursive=TRUE)
#' 
#' @export
#' @name saveBaseFactor
#' @aliases 
#' saveObject,factor-method
#' stageObject,factor-method
setMethod("saveObject", "factor", function(x, path, ...) {
    dir.create(path, showWarnings=FALSE)
    ofile <- file.path(path, "contents.h5")

    fhandle <- H5Fcreate(ofile, "H5F_ACC_TRUNC")
    on.exit(H5Fclose(fhandle), add=TRUE, after=FALSE)
    ghandle <- H5Gcreate(fhandle, "string_factor")
    on.exit(H5Gclose(ghandle), add=TRUE, after=FALSE)

    if (is.ordered(x)) {
        h5_write_attribute(ghandle, "ordered", 1L, scalar=TRUE)
    }

    .simple_save_codes(ghandle, x)
    h5_write_vector(ghandle, "levels", levels(x))

    saveObjectFile(path, "string_factor", list(string_factor=list(version="1.0")))
    invisible(NULL)
})

.simple_save_codes <- function(ghandle, x, save.names=TRUE) {
    codes <- as.integer(x) - 1L

    missing.placeholder <- NULL
    if (anyNA(codes)) {
        missing.placeholder <- nlevels(x)
        codes[is.na(codes)] <- missing.placeholder
    }

    dhandle <- h5_write_vector(ghandle, "codes", codes, type="H5T_NATIVE_UINT32", emit=TRUE)
    on.exit(H5Dclose(dhandle), add=TRUE, after=FALSE)

    if (!is.null(missing.placeholder)) {
        h5_write_attribute(dhandle, missingPlaceholderName, missing.placeholder, type="H5T_NATIVE_UINT32", scalar=TRUE)
    }

    if (save.names && !is.null(names(x))) {
        h5_write_vector(ghandle, "names", names(x))
    }
}

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
#' @importFrom S4Vectors DataFrame
setMethod("stageObject", "factor", function(x, dir, path, child = FALSE, ...) {
    dir.create(file.path(dir, path), showWarnings=FALSE)

    contents <- as.integer(x) - 1L
    mock <- DataFrame(values=contents)
    if (!is.null(names(x))) {
        mock <- cbind(names=names(x), mock)
    }

    new_path <- paste0(path, "/indices.txt.gz")
    ofile <- file.path(dir, new_path)
    quickWriteCsv(mock, ofile, row.names=FALSE, compression="gzip", validate=FALSE)

    level_meta <- stageObject(levels(x), dir, paste0(path, "/levels"), child=TRUE)
    level_stub <- writeMetadata(level_meta, dir)

    list(
        `$schema` = "string_factor/v1.json",
        path = new_path,
        is_child = child,
        factor = list(
            length = length(x),
            names = !is.null(names(x)),
            compression = "gzip"
        ),
        string_factor = list(
            levels = list(resource = level_stub),
            ordered = is.ordered(x)
        )
    )
})
