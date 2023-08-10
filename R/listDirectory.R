#' List staged objects in a directory
#'
#' List all objects in a staging directory by loading their metadata.
#' 
#' @param dir String containing a path to a staging directory.
#' @param ignore.children Logical scalar indicating whether to ignore metadata for child objects.
#' 
#' @return Named list where each entry is itself a list containing the metadata for an object.
#' The name of the entry is the path inside \code{dir} to the object.
#'
#' If \code{ignore.children=TRUE}, metadata is only returned for non-child objects or redirections.
#'
#' @author Aaron Lun
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
#' all.meta <- listDirectory(tmp)
#' names(all.meta) 
#'
#' @export
#' @aliases listAllObjects
#' @importFrom jsonlite fromJSON
listDirectory <- function(dir, ignore.children = TRUE) {
    all.json <- list.files(dir, pattern="\\.json$", recursive=TRUE)
    out <- lapply(file.path(dir, all.json), fromJSON, simplifyVector=FALSE)
    names(out) <- vapply(out, function(x) x$path, "")

    if (ignore.children) {
        child <- vapply(out, function(x) isTRUE(x$is_child), TRUE)
        out <- out[!child]
    }

    out
}

# Soft-deprecated back-compatibility fixes.

#' @export
listLocalObjects <- function(..., ignoreChildren = TRUE) listDirectory(..., ignore.children = ignoreChildren)
