#' Load a factor
#'
#' Load a base R \link{factor} from file.
#'
#' @inheritParams loadDataFrame
#' @param ... Further arguments, ignored.
#'
#' @return The vector described by \code{info}, possibly with names.
#'
#' @seealso
#' \code{"\link{stageObject,factor-method}"}, for the staging method.
#'
#' @author Aaron Lun
#' 
#' @examples
#' tmp <- tempfile()
#' dir.create(tmp)
#' meta <- stageObject(factor(letters[1:10], letters), tmp, path="bar")
#' loadBaseFactor(meta, tmp)
#' 
#' @export
loadBaseFactor <- function(info, project, ...) {
    fpath <- acquireFile(project, info$path)
    meta <- info$factor

    df <- read.csv3(fpath, compression=meta$compression, nrows=meta$length)
    codes <- df[,ncol(df)]

    smeta <- info$string_factor
    level_meta <- acquireMetadata(project, smeta$levels$resource$path)
    levels <- altLoadObject(level_meta, project=project)

    output <- factor(levels[codes], levels=levels, ordered=isTRUE(smeta$ordered))
    if (isTRUE(meta$names)) {
        names(output) <- df[,1]
    }
    output 
}
