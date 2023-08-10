#' Alter the loading function
#'
#' Allow alabaster applications to specify an alternative loading function in \code{\link{altLoadObject}}.
#' 
#' @param ... Further arguments to pass to \code{\link{loadObject}} or its equivalent.
#' @param load Function that can serve as a drop-in replacement for \code{\link{loadObject}}. 
#'
#' @return
#' For \code{altLoadObject}, any R object similar to those returned by \code{\link{loadObject}}.
#'
#' For \code{altLoadObjectFunction}, the alternative function (if any) is returned if \code{load} is missing.
#' If \code{load} is provided, it is used to define the alternative, and the previous alternative is returned.
#'
#' @details
#' \code{altLoadObject} is just a wrapper around \code{\link{loadObject}} that responds to any setting of \code{altLoadObjectFunction}.
#' This allows alabaster applications to inject customizations into the loading process, e.g., to add more metadata to particular objects.
#' Developers of alabaster extensions should use \code{altLoadObject} (instead of \code{loadObject}) to load child objects when writing their own loading functions,
#' to ensure that application-specific customizations are respected for the children.
#'
#' To motivate the use of \code{altLoadObject}, consider the following scenario.
#' \enumerate{
#' \item We have created a loading function \code{loadX} function to load an instance of class X in an alabaster extension.
#' This function may be called by \code{\link{loadObject}} if instances of X are children of other objects.
#' \item An alabaster application Y requires the addition of some custom metadata during the loading process for X.
#' It defines an alternative loading function \code{loadObject2} that, upon encountering a schema for X, redirects to a application-specific loader \code{loadX2}.
#' An example implementation for \code{loadX2} would involve calling \code{loadX} and decorating the result with the extra metadata.
#' \item When operating in the context of application Y, the \code{loadObject2} generic is used to set \code{altLoadObjectFunction}.
#' Any calls to \code{altLoadObject} in Y's context will subsequently call \code{loadObject2}.
#' \item So, when writing a loading function in an alabaster extension for a class that might contain X as children, 
#' we use \code{\link{altLoadObject}} instead of directly using \code{\link{loadObject}}. 
#' This ensures that, if a child instance of X is encountered \emph{and} we are operating in the context of application Y, 
#' we correctly call \code{loadObject2} and then ultimately \code{loadX2}.
#' }
#'
#' Note for application developers: \code{loadX2} should \emph{not} call \code{altLoadObject} on the same instance of X.
#' Doing so will introduce an infinite recursion where \code{altLoadObject} calls \code{loadX2} that then calls \code{altLoadObject}, etc.
#' Rather, application developers should either call \code{loadObject} or \code{loadX} directly.
#' For child objects, no infinite recursion will occur and either \code{loadObject2} or \code{altLoadObject} can be used. 
#'
#' @author Aaron Lun
#' @examples
#' old <- altLoadObjectFunction()
#'
#' # Setting it to something.
#' altLoadObjectFunction(function(...) {
#'     print("YAY")
#'     loadObject(...) 
#' })
#'
#' # Staging an example DataFrame:
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#' tmp <- tempfile()
#' dir.create(tmp)
#' out <- stageObject(df, tmp, path="coldata")
#'
#' # And now loading it - this should print our message.
#' altLoadObject(out, tmp)
#' 
#' # Restoring the old loader:
#' altLoadObjectFunction(old)
#'
#' @export
#' @aliases .altLoadObject .loadObject
altLoadObject <- function(...) {
    FUN <- altLoadObjectFunction()
    if (is.null(FUN)) {
        FUN <- loadObject
    }
    FUN(...)
}

#' @export
#' @rdname altLoadObject
altLoadObjectFunction <- (function() {
    cur.env <- new.env()
    cur.env$loader <- NULL
    function(load) {
        prev <- cur.env$loader
        if (missing(load)) {
            prev
        } else {
            cur.env$loader <- load
            invisible(prev)
        }
    }
})()

# Soft-deprecated back-compatibility fixes. 

#' @export
.loadObject <- function(...) altLoadObject(...)

#' @export
.altLoadObject <- function(...) altLoadObjectFunction(...)
