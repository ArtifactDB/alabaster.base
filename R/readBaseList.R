#' Read a base list from disk
#'
#' Read a \link{list} from its on-disk representation.
#' This is usually not directly called by users, but is instead called by dispatch in \code{\link{readObject}}.
#'
#' @param path String containing a path to a directory, itself created with the list method for \code{\link{stageObject}}. 
#' @param metadata Named list containing metadata for the object, see \code{\link{readObjectFile}} for details.
#' @param list.parallel Whether to perform reading and parsing in parallel for greater speed.
#' Only relevant for lists stored in the JSON format.
#' @param ... Further arguments to be passed to \code{\link{altReadObject}} for complex child objects.
#' 
#' @return 
#' The list represented by \code{path}.
#'
#' @author Aaron Lun
#'
#' @seealso
#' \code{"\link{stageObject,list-method}"}, for the staging method.
#'
#' @examples
#' library(S4Vectors)
#' ll <- list(A=1, B=LETTERS, C=DataFrame(X=letters))
#'
#' tmp <- tempfile()
#' saveObject(ll, tmp)
#' readObject(tmp)
#'
#' @export
#' @aliases loadBaseList
readBaseList <- function(path, metadata, list.parallel=TRUE, ...) {
    all.children <- list()
    child.path <- file.path(path, "other_contents")
    if (file.exists(child.path)) {
        all.children <- as.list(list.files(child.path))
        for (i in seq_along(all.children)) {
            all.children[[i]] <- altReadObject(file.path(child.path, all.children[[i]]), list.parallel=list.parallel, ...)
        }
    }

    format <- metadata$simple_list$format
    if (is.null(format) || format == "hdf5") {
        lpath <- file.path(path, "list_contents.h5")
        output <- load_list_hdf5(lpath, "simple_list", all.children)
    } else {
        lpath <- file.path(path, "list_contents.json.gz")
        output <- load_list_json(lpath, all.children, list.parallel)
    }

    output
}

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
loadBaseList <- function(info, project, parallel=TRUE) {
    children <- info$simple_list$children
    for (i in seq_along(children)) {
        child.meta <- acquireMetadata(project, children[[i]]$resource$path) 
        children[[i]] <- altLoadObject(child.meta, project=project)
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

    # Need to replace any Rfc3339's with POSIXct objects for back-compatibility.
    # This is done manually because the C++ code already uses Rfc3339 and I don't
    # want to have to compile a separate version just to handle the old case.
    to_posix <- function(x) {
        if (is.list(x) && class(x)[1] == "list") {
            lapply(x, to_posix)
        } else if (is.Rfc3339(x)) {
            as.POSIXct(x)
        } else {
            x
        }
    }

    to_posix(output)
}
