#' List objects in a directory
#'
#' List all objects in a directory, along with their types.
#' 
#' @param dir String containing a path to a staging directory.
#' @param ignore.children Logical scalar indicating whether to ignore child objects.
#' 
#' @return \linkS4class{DataFrame} where each row corresponds to an object and contains;
#' \itemize{
#' \item \code{path}, the relative path to the object's subdirectory inside \code{dir}.
#' \item \code{type}, the type of the object
#' \item \code{child}, whether or not the object is a child of another object.
#' }
#'
#' If \code{ignore.children=TRUE}, metadata is only returned for non-child objects. 
#'
#' @author Aaron Lun
#'
#' @examples
#' tmp <- tempfile()
#' dir.create(tmp)
#'
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#' saveObject(df, file.path(tmp, "whee"))
#'
#' ll <- list(A=1, B=LETTERS, C=DataFrame(X=1:5))
#' saveObject(ll, file.path(tmp, "stuff"))
#'
#' listObjects(tmp)
#' listObjects(tmp, include.children=TRUE)
#'
#' @export
#' @aliases listLocalObjects listDirectory
#' @importFrom S4Vectors DataFrame
listObjects <- function(dir, include.children=FALSE) {
    DataFrame(.traverse_directory_listing(dir, ".", include.children=include.children))
}

.traverse_directory_listing <- function(root, dir, already.child=FALSE, include.children=FALSE) {
    fpath <- file.path(root, dir, "OBJECT")
    is.obj <- file.exists(fpath)
    if (is.obj) {
        paths <- dir
        types <- readLines(fpath)
        childs <- already.child
    } else {
        paths <- character(0)
        types <- character(0)
        childs <- logical(0)
    }

    if (include.children || !is.obj) {
        more.dirs <- list.dirs(file.path(root, dir), recursive=FALSE, full.names=FALSE)
        for (k in more.dirs) {
            if (dir != ".") {
                subdir <- file.path(dir, k)
            } else {
                subdir <- k
            }

            sub <- .traverse_directory_listing(root, subdir, already.child=(already.child || is.obj), include.children=include.children)
            paths <- c(paths, sub$path)
            types <- c(types, sub$type)
            childs <- c(childs, sub$child)
        }
    }

    list(path=paths, type=types, child=childs)
}


#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
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
