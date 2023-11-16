#' Remove a non-child object from the staging directory
#'
#' \emph{WARNING: this function is deprecated, as directories of non-child objects can just be deleted with regular methods (e.g., \code{\link{file.rename}}) in the latest version of \pkg{alabaster}.}
#' Pretty much as it says in the title.
#' This only works with non-child objects as children are referenced by their parents and cannot be safely removed in this manner.
#'
#' @param dir String containing the path to the staging directory.
#' @param path String containing the path to a non-child object inside \code{dir}, as used in \code{\link{acquireMetadata}}.
#' This can also be a redirection to such an object.
#' 
#' @author Aaron Lun
#'
#' @return
#' The object represented by \code{path} is removed, along with any redirections to it.
#' A \code{NULL} is invisibly returned.
#'
#' @details
#' This function will search around \code{path} for JSON files containing redirections to \code{path}, and remove them.
#' More specifically, if \code{path} is a subdirectory, it will search in the same directory containing \code{path};
#' otherwise, it will search in the directory containing \code{dirname(path)}.
#' Redirections in other locations will not be removed automatically - these will be caught by \code{\link{checkValidDirectory}} and should be manually removed.
#' 
#' @examples
#' tmp <- tempfile()
#' dir.create(tmp)
#'
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#' meta <- stageObject(df, tmp, path="whee")
#' writeMetadata(meta, tmp)
#'
#' ll <- list(A=1, B=LETTERS, C=DataFrame(X=1:5))
#' meta <- stageObject(ll, tmp, path="stuff")
#' writeMetadata(meta, tmp)
#'
#' redirect <- createRedirection(tmp, "whoop", "whee/simple.csv.gz")
#' writeMetadata(redirect, tmp)
#'
#' list.files(tmp, recursive=TRUE)
#' removeObject(tmp, "whoop")
#' list.files(tmp, recursive=TRUE)
#'
#' @export
removeObject <- function(dir, path) {
    meta <- acquireMetadata(dir, path)

    if (isTRUE(meta$is_child)) {
        stop("cannot remove a child object without removing the parent")
    }

    refpath <- meta$path
    unlink(file.path(dir, dirname(refpath)), recursive=TRUE)

    # Searching for redirections.
    searchpath <- file.path(dir, dirname(dirname(refpath)))
    potential <- list.files(searchpath, pattern="\\.json$")

    for (x in potential) {
        redpath <- file.path(searchpath, x)
        meta <- fromJSON(redpath, simplifyVector=FALSE)

        if (dirname(meta[["$schema"]]) == "redirection") {
            survivors <- list()
            for (x in meta[["redirection"]][["targets"]]) {
                if (x$type != "local" || x$location != refpath) {
                    survivors <- c(survivors, list(x))
                }
            }

            if (length(survivors)) {
                meta[["redirection"]][["targets"]] <- survivors
                writeMetadata(meta, dir)
            } else {
                unlink(redpath)
            }
        }
    }

    invisible(NULL)
}
