#' Load an DataFrame factor
#'
#' Load a \linkS4class{Factor} where the levels are rows of a \linkS4class{DataFrame}.
#'
#' @inheritParams loadDataFrame
#'
#' @return A \linkS4class{DataFrameFactor} described by \code{info}.
#'
#' @author Aaron Lun
#'
#' @examples
#' library(S4Vectors)
#' df <- DataFrame(X=LETTERS[1:5], Y=1:5)
#' out <- DataFrameFactor(df[sample(5, 100, replace=TRUE),,drop=FALSE])
#' 
#' # Staging the object:
#' tmp <- tempfile()
#' dir.create(tmp)
#' info <- stageObject(out, tmp, path="test")
#'
#' # Loading it back in:
#' loadDataFrameFactor(info, tmp)
#'
#' @export
#' @importFrom S4Vectors DataFrameFactor
loadDataFrameFactor <- function(info, project) {
    lev.info <- acquireMetadata(project, info$data_frame_factor$levels$resource$path)
    levels <- .loadObject(lev.info, project=project)

    path <- acquireFile(project, info$path)
    has.names <- !is.null(info$factor$names)
    idx <- .quickReadCsv(path, 
         c(index="integer"), 
         row.names=has.names, 
         compression=info$factor$compression,
         expected.nrows=info$factor$length
    )

    indexes <- idx$index
    if (has.names) {
        names(indexes) <- rownames(idx)
    }

    DataFrameFactor(index=indexes, levels=levels)
}
