#' Convenience helpers for handling local directories
#'
#' Read, write and list objects from a local staging directory.
#' These are just convenience wrappers around functions like \code{\link{loadObject}}, \code{\link{stageObject}} and \code{\link{.writeMetadata}}.
#'
#' @param x Object to be saved.
#' @param dir String containing a path to the directory.
#' @param path String containing a relative path to the object of interest inside \code{dir}.
#' @param includeChildren Logical scalar indicating whether child objects should be returned.
#'
#' @return For \code{readLocalObject}, the object at \code{path}.
#'
#' For \code{saveLocalObject}, the object is saved to \code{path} inside \code{dir}.
#' All necessary directories are created if they are not already present.
#' A \code{NULL} is returned invisibly.
#'
#' For \code{listLocalObjects}, a named list where each entry corresponds to an object and is named after the path to that object inside \code{dir}.
#' The value of each element is another list containing the metadata for its corresponding object.
#'
#' @author Aaron Lun
#'
#' @examples
#' local <- tempfile()
#'
#' # Creating a slightly complicated object:
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#' df$C <- DataFrame(D=letters[1:10], E=runif(10))
#'
#' # Saving it:
#' saveLocalObject(df, local, "FOOBAR")
#'
#' # Reading it back:
#' readLocalObject(local, "FOOBAR")
#'
#' # Listing all available objects:
#' names(listLocalObjects(local))
#' names(listLocalObjects(local, includeChildren=TRUE))
#' @export
readLocalObject <- function(dir, path, ...) {
    meta <- acquireMetadata(dir, path)
    loadObject(meta, dir, ...)
}

#' @export
#' @rdname readLocalObject
saveLocalObject <- function(x, dir, path, ...) {
    dir.create(file.path(dir, dirname(path)), recursive=TRUE, showWarnings=FALSE)
    meta <- stageObject(x, dir, path, ...)
    info <- .writeMetadata(meta, dir)
    .writeMetadata(.createRedirection(dir, path, info$path), dir)
    invisible(NULL)
}

#' @export
#' @rdname readLocalObject
#' @importFrom jsonlite fromJSON
listLocalObjects <- function(dir, includeChildren=FALSE) {
    all.json <- list.files(dir, recursive=TRUE, pattern="\\.json$")
    all.paths <- list()
    for (x in all.json) {
        info <- fromJSON(file.path(dir, x), simplifyVector=FALSE)
        if (includeChildren || !isTRUE(info$is_child)) {
            all.paths[[info$path]] <- info
        }
    }
    all.paths
}
