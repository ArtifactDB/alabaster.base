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
#' @name stageBaseFactor
#' @importFrom S4Vectors DataFrame
setMethod("stageObject", "factor", function(x, dir, path, child = FALSE, ...) {
    dir.create(file.path(dir, path), showWarnings=FALSE)
    new_path <- paste0(path, "/indices.txt.gz")

    contents <- as.integer(x)
    mock <- DataFrame(values=contents)
    if (!is.null(names(x))) {
        mock <- cbind(names=names(x), mock)
    }
    quickWriteCsv(mock, file.path(dir, new_path), row.names=FALSE, compression="gzip")

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
