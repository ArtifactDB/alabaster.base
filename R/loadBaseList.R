#' Load a base list
#'
#' Load a \link{list} from file, possibly containing complex entries.
#'
#' @inheritParams loadDataFrame
#' 
#' @return The list described by \code{info}.
#'
#' @author Aaron Lun
#'
#' @details
#' This function effectively reverses the behavior of the list method for \code{\link{stageObject}}, loading the \link{list} back into memory from the JSON file.
#' Atomic vectors, arrays and data frames are loaded directly while complex values are loaded by calling the appropriate loading function.
#' 
#' @examples
#' library(S4Vectors)
#' ll <- list(A=1, B=LETTERS, C=DataFrame(X=letters))
#'
#' # First staging it:
#' tmp <- tempfile()
#' dir.create(tmp)
#' info <- stageObject(ll, tmp, path="stuff")
#'
#' # And now loading it:
#' loadBaseList(info, tmp)
#'
#' @export
loadBaseList <- function(info, project) {
    lpath <- acquireFile(project, info$path)
    contents <- paste(readLines(lpath), collapse="\n")

    children <- info$basic_list$children
    for (i in seq_along(children)) {
        child.meta <- acquireMetadata(project, children[[i]]$resource$path) 
        children[[i]] <- .loadObject(child.meta, project=project)
    }

    load_list(contents, children)
}
