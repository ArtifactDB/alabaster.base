#' Stage factors
#'
#' Pretty much as it says, let's stage a base R \link{factor}.
#'
#' @param x Any of the assorted simple vector types.
#' @inheritParams stageObject
#' @param ... Further arguments that are ignored.
#'
#' @return
#' A named list containing the metadata for \code{x}.
#' \code{x} itself is written to a file inside \code{path}, along with the various levels.
#'
#' @author Aaron Lun
#' 
#' @examples
#' tmp <- tempfile()
#' dir.create(tmp)
#' stageObject(factor(1:10, 1:30), tmp, path="foo")
#' list.files(tmp, recursive=TRUE)
#' 
#' @rdname stageBaseFactor
#' @importFrom S4Vectors DataFrame
setMethod("stageObject", "factor", function(x, dir, path, simplified = FALSE, child = FALSE, ...) {
    dir.create(file.path(dir, path), showWarnings=FALSE)

    if (simplified) {
        ofile <- file.path(dir, path, "contents.h5")
        h5createFile(ofile)
        host <- "string_factor"
        h5createGroup(ofile, host)

        fhandle <- H5Fopen(ofile)
        on.exit(H5Fclose(fhandle), add=TRUE)
        (function (){
            ghandle <- H5Gopen(fhandle, host)
            on.exit(H5Gclose(ghandle), add=TRUE)
            h5writeAttribute("1.0", ghandle, "version", asScalar=TRUE)
        })()

        .simple_save_codes(fhandle, host, x)
        h5write(levels(x), fhandle, paste0(host, "/levels"))
        write("string_factor", file=file.path(dir, path, "OBJECT"))

    } else {
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
    }
})

.simple_save_codes <- function(fhandle, host, x) {
    codes <- as.integer(x) - 1L

    missing.placeholder <- NULL
    if (anyNA(codes)) {
        missing.placeholder <- -1L
        codes[is.na(codes)] <- missing.placeholder
    }

    full.data.name <- paste0(host, "/codes")
    h5write(codes, fhandle, full.data.name)
    if (!is.null(missing.placeholder)) {
        addMissingPlaceholderAttributeForHdf5(fhandle, full.data.name, missing.placeholder)
    }

    if (!is.null(names(x))) {
        h5write(names(x), fhandle, paste0(host, "/names"))
    }
}
