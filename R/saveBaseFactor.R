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
    h5createFile(ofile)
    host <- "string_factor"
    h5createGroup(ofile, host)

    fhandle <- H5Fopen(ofile)
    on.exit(H5Fclose(fhandle), add=TRUE)
    ghandle <- H5Gopen(fhandle, host)
    on.exit(H5Gclose(ghandle), add=TRUE, after=FALSE)
    (function (){
        h5writeAttribute("1.0", ghandle, "version", asScalar=TRUE)
        if (is.ordered(x)) {
            h5writeAttribute(1L, ghandle, "ordered", asScalar=TRUE)
        }
    })()

    .simple_save_codes(ghandle, x)
    h5write(levels(x), ghandle, "levels")

    write("string_factor", file=file.path(path, "OBJECT"))
    invisible(NULL)
})

.simple_save_codes <- function(ghandle, x, save.names=TRUE) {
    codes <- as.integer(x) - 1L

    missing.placeholder <- NULL
    if (anyNA(codes)) {
        missing.placeholder <- nlevels(x)
        codes[is.na(codes)] <- missing.placeholder
    }

    shandle <- H5Screate_simple(length(x))
    on.exit(H5Sclose(shandle), add=TRUE)
    dhandle <- H5Dcreate(ghandle, "codes", dtype_id="H5T_NATIVE_UINT32", h5space=shandle)
    on.exit(H5Dclose(dhandle), add=TRUE, after=FALSE)
    H5Dwrite(dhandle, codes)

    if (!is.null(missing.placeholder)) {
        ashandle <- H5Screate("H5S_SCALAR")
        on.exit(H5Sclose(ashandle), add=TRUE, after=FALSE)
        ahandle <- H5Acreate(dhandle, "missing-value-placeholder", dtype_id="H5T_NATIVE_UINT32", h5space=ashandle)
        on.exit(H5Aclose(ahandle), add=TRUE, after=FALSE)
        H5Awrite(ahandle, missing.placeholder)
    }

    if (save.names && !is.null(names(x))) {
        h5write(names(x), ghandle, "names")
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
