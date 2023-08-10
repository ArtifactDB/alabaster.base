#' Convenience helpers for handling local directories
#'
#' Read and write objects from a local staging directory.
#' These are just convenience wrappers around functions like \code{\link{loadObject}}, \code{\link{stageObject}} and \code{\link{writeMetadata}}.
#'
#' @param x Object to be saved.
#' @param dir String containing a path to the directory.
#' @param path String containing a relative path to the object of interest inside \code{dir}.
#' @param includeChildren Logical scalar indicating whether child objects should be returned.
#' @param ... Further arguments to pass to \code{\link{loadObject}} (for \code{quickLoadObject}) or \code{\link{stageObject}} (for \code{quickStageObject}).
#'
#' @return For \code{quickLoadObject}, the object at \code{path}.
#'
#' For \code{quickStageObject}, the object is saved to \code{path} inside \code{dir}.
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
#' quickStageObject(df, local, "FOOBAR")
#'
#' # Reading it back:
#' quickLoadObject(local, "FOOBAR")
#'
#' @export
#' @aliases readLocalObject saveLocalObject
quickLoadObject <- function(dir, path, ...) {
    meta <- acquireMetadata(dir, path)
    loadObject(meta, dir, ...)
}

#' @export
#' @rdname quickLoadObject
quickStageObject <- function(x, dir, path, ...) {
    dir.create(file.path(dir, dirname(path)), recursive=TRUE, showWarnings=FALSE)
    meta <- stageObject(x, dir, path, ...)
    info <- writeMetadata(meta, dir)
    writeMetadata(.createRedirection(dir, path, info$path), dir)
    invisible(NULL)
}

# Soft-deprecated back-compatibility fixes.

#' @export
readLocalObject <- function(...) quickLoadObject(...)

#' @export
saveLocalObject <- function(...) quickStageObject(...)
