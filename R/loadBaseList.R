#' Load a base list
#'
#' Load a \link{list} from file, possibly containing complex entries.
#'
#' @inheritParams loadDataFrame
#' @param parallel Whether to perform reading and parsing in parallel for greater speed.
#' Only relevant for lists stored in the JSON format.
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
loadBaseList <- function(info, project, parallel=TRUE) {
    children <- info$simple_list$children
    for (i in seq_along(children)) {
        child.meta <- acquireMetadata(project, children[[i]]$resource$path) 
        children[[i]] <- .loadObject(child.meta, project=project)
    }

    lpath <- acquireFile(project, info$path)

    output <- NULL
    if ("hdf5_simple_list" %in% names(info)) {
        output <- load_list_hdf5(lpath, info$hdf5_simple_list$group, children)
    } else {
        comp <- info$json_simple_list$compression
        if (!is.null(comp) && !(comp %in% c("none", "gzip"))) {
            stop("only uncompressed or Gzip-compressed JSON lists are supported")
        }
        output <- load_list_json(lpath, children, parallel)
    }

    output
}
