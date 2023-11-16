#' Alter the saving generic
#'
#' Allow alabaster applications to divert to a different saving generic instead of \code{\link{saveObject}}.
#'
#' @param ... Further arguments to pass to \code{\link{saveObject}} or an equivalent generic.
#' @param generic Generic function that can serve as a drop-in replacement for \code{\link{saveObject}}. 
#'
#' @return 
#' For \code{altSaveObject}, files are created at the specified location, see \code{\link{saveObject}} for details.
#'
#' For \code{altSaveObjectFunction}, the alternative generic (if any) is returned if \code{generic} is missing.
#' If \code{generic} is provided, it is used to define the alternative, and the previous alternative is returned.
#'
#' @details
#' \code{altSaveObject} is just a wrapper around \code{\link{saveObject}} that responds to any setting of \code{altSaveObjectFunction}.
#' This allows applications to inject customizations into the staging process, e.g., to store more metadata to particular objects.
#' Developers of alabaster extensions should use \code{altSaveObject} to save child objects when writing their \code{saveObject} methods,
#' to ensure that application-specific customizations are respected for the children.
#'
#' To motivate the use of \code{altSaveObject}, consider the following scenario.
#' \enumerate{
#' \item We have created a staging method for class X, defined for the \code{\link{saveObject}} generic.
#' \item An alabaster application Y requires the addition of some custom metadata during the staging process for X.
#' It defines an alternative staging generic \code{saveObject2} that, upon encountering an instance of X, redirects to a application-specific method.
#' For example, the \code{saveObject2} method for X could call X's \code{saveObject} method and add the necessary metadata to the result.
#' \item When operating in the context of application Y, the \code{saveObject2} generic is used to set \code{altSaveObjectFunction}.
#' Any calls to \code{altSaveObject} in Y's context will subsequently call \code{saveObject2}.
#' \item So, when writing a \code{saveObject} method for any objects that might contain an instance of X as a child, 
#' we call \code{\link{altSaveObject}} on that X object instead of directly using \code{\link{saveObject}}. 
#' This ensures that, if a child instance of X is encountered \emph{and} we are operating in the context of application Y, 
#' we correctly call \code{saveObject2} and then ultimately the application-specific method.
#' }
#' 
#' @author Aaron Lun
#' @examples
#' old <- altSaveObjectFunction()
#'
#' # Creating a new generic for demonstration purposes:
#' setGeneric("superSaveObject", function(x, path, ...)
#'     standardGeneric("superSaveObject"))
#'
#' setMethod("superSaveObject", "ANY", function(x, path, ...) {
#'     print("Falling back to the base method!")
#'     saveObject(x, path, ...)
#' })
#' 
#' altSaveObjectFunction(superSaveObject)
#'
#' # Staging an example DataFrame. This should print our message.
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#' tmp <- tempfile()
#' dir.create(tmp)
#' out <- altSaveObject(df, tmp, path="coldata")
#' 
#' # Restoring the old loader:
#' altSaveObjectFunction(old)
#'
#' @export
#' @aliases .stageObject .altStageObject altStageObject altStageObjectFunction
altSaveObject <- function(...) {
    FUN <- altSaveObjectFunction()
    if (is.null(FUN)) {
        FUN <- saveObject
    }
    FUN(...)
}

#' @export
#' @rdname altSaveObject
altSaveObjectFunction <- (function() {
    cur.env <- new.env()
    cur.env$generic <- NULL
    function(generic) {
        prev <- cur.env$generic
        if (missing(generic)) {
            prev
        } else {
            cur.env$generic <- generic
            invisible(prev)
        }
    }
})()

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
altStageObject <- function(...) {
    FUN <- altStageObjectFunction()
    if (is.null(FUN)) {
        FUN <- stageObject
    }
    FUN(...)
}

#' @export
altStageObjectFunction <- (function() {
    cur.env <- new.env()
    cur.env$generic <- NULL
    function(generic) {
        prev <- cur.env$generic
        if (missing(generic)) {
            prev
        } else {
            cur.env$generic <- generic
            invisible(prev)
        }
    }
})()

# Soft-deprecated back-compatibility fixes.

#' @export
.stageObject <- function(...) altStageObject(...)

#' @export
.altStageObject <- function(...) altStageObjectFunction(...)

