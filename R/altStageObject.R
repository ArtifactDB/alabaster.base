#' Alter the staging generic
#'
#' Allow alabaster applications to divert to a different staging generic from \code{\link{stageObject}}.
#'
#' @param ... Further arguments to pass to \code{\link{stageObject}} or an equivalent generic.
#' @param generic Generic function that can serve as a drop-in replacement for \code{\link{stageObject}}. 
#'
#' @return 
#' For \code{altStageObject}, a named list similar to the return value of \code{\link{stageObject}}. 
#'
#' For \code{altStageObjectFunction}, the alternative generic (if any) is returned if \code{generic} is missing.
#' If \code{generic} is provided, it is used to define the alternative, and the previous alternative is returned.
#'
#' @details
#' \code{altStageObject} is just a wrapper around \code{\link{stageObject}} that responds to any setting of \code{altStageObjectFunction}.
#' This allows applications to inject customizations into the staging process, e.g., to store more metadata to particular objects.
#' Developers of alabaster extensions should use \code{altStageObject} to stage child objects when writing their \code{stageObject} methods,
#' to ensure that application-specific customizations are respected for the children.
#'
#' To motivate the use of \code{altStageObject}, consider the following scenario.
#' \enumerate{
#' \item We have created a staging method for class X, defined for the \code{\link{stageObject}} generic.
#' \item An alabaster application Y requires the addition of some custom metadata during the staging process for X.
#' It defines an alternative staging generic \code{stageObject2} that, upon encountering an instance of X, redirects to a application-specific method.
#' For example, the \code{stageObject2} method for X could call X's \code{stageObject} method, add the necessary metadata to the result, and then return the list.
#' \item When operating in the context of application Y, the \code{stageObject2} generic is used to set \code{altStageObjectFunction}.
#' Any calls to \code{altStageObject} in Y's context will subsequently call \code{stageObject2}.
#' \item So, when writing a \code{stageObject} method for any objects that might include X as children, we use \code{\link{altStageObject}} instead of directly using \code{\link{stageObject}}. 
#' This ensures that, if a child instance of X is encountered \emph{and} we are operating in the context of application Y, 
#' we correctly call \code{stageObject2} and then ultimately the application-specific method.
#' }
#'
#' Note for application developers: the alternative \code{stageObject2} method for X should \emph{not} call \code{altStageObject} on the same instance of X.
#' Doing so will introduce an infinite recursion where \code{altStageObject} calls \code{stageObject2} that then calls \code{altStageObject}, etc.
#' Rather, developers should call \code{stageObject} directly in such cases.
#' For child objects, no infinite recursion will occur and either \code{stageObject2} or \code{altStageObject} can be used. 
#' 
#' @author Aaron Lun
#' @examples
#' old <- altStageObjectFunction()
#'
#' # Creating a new generic for demonstration purposes:
#' setGeneric("superStageObject", function(x, dir, path, child=FALSE, ...)
#'     standardGeneric("superStageObject"))
#'
#' setMethod("superStageObject", "ANY", function(x, dir, path, child=FALSE, ...) {
#'     print("Falling back to the base method!")
#'     stageObject(x, dir, path, child=child, ...)
#' })
#' 
#' altStageObjectFunction(superStageObject)
#'
#' # Staging an example DataFrame. This should print our message.
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#' tmp <- tempfile()
#' dir.create(tmp)
#' out <- altStageObject(df, tmp, path="coldata")
#' 
#' # Restoring the old loader:
#' altStageObjectFunction(old)
#'
#' @export
#' @aliases .stageObject .altStageObject
altStageObject <- function(...) {
    FUN <- altStageObjectFunction()
    if (is.null(FUN)) {
        FUN <- stageObject
    }
    FUN(...)
}

#' @export
#' @rdname altStageObject
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

