#' Alter the reading function
#'
#' Allow alabaster applications to specify an alternative reading function in \code{\link{altReadObject}}.
#' 
#' @param ... Further arguments to pass to \code{\link{readObject}} or its equivalent.
#' @param fun Function that can serve as a drop-in replacement for \code{\link{readObject}}. 
#'
#' @return
#' For \code{altReadObject}, any R object similar to those returned by \code{\link{readObject}}.
#'
#' For \code{altReadObjectFunction}, the alternative function (if any) is returned if \code{fun} is missing.
#' If \code{fun} is provided, it is used to define the alternative, and the previous alternative is returned.
#'
#' @details
#' \code{altReadObject} is just a wrapper around \code{\link{readObject}} that responds to any setting of \code{altReadObjectFunction}.
#' This allows alabaster applications to inject customizations into the reading process, e.g., to add more metadata to particular objects.
#' Developers of alabaster extensions should use \code{altReadObject} (instead of \code{readObject}) to read child objects when writing their own reading functions,
#' to ensure that application-specific customizations are respected for the children.
#'
#' To motivate the use of \code{altReadObject}, consider the following scenario.
#' \enumerate{
#' \item We have created a reading function \code{readX} function to read an instance of class X in an alabaster extension.
#' This function may be called by \code{\link{readObject}} if instances of X are children of other objects.
#' \item An alabaster application Y requires the addition of some custom metadata during the reading process for X.
#' It defines an alternative reading function \code{readObject2} that, upon encountering a schema for X, redirects to a application-specific reader \code{readX2}.
#' An example implementation for \code{readX2} would involve calling \code{readX} and decorating the result with the extra metadata.
#' \item When operating in the context of application Y, the \code{readObject2} generic is used to set \code{altReadObjectFunction}.
#' Any calls to \code{altReadObject} in Y's context will subsequently call \code{readObject2}.
#' \item So, when writing a reading function in an alabaster extension for a class that might contain instances of X as children, 
#' we use \code{\link{altReadObject}} instead of directly using \code{\link{readObject}}. 
#' This ensures that, if a child instance of X is encountered \emph{and} we are operating in the context of application Y, 
#' we correctly call \code{readObject2} and then ultimately \code{readX2}.
#' }
#'
#' @author Aaron Lun
#' @examples
#' old <- altReadObjectFunction()
#'
#' # Setting it to something.
#' altReadObjectFunction(function(...) {
#'     print("YAY")
#'     readObject(...) 
#' })
#'
#' # Staging an example DataFrame:
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#' tmp <- tempfile()
#' saveObject(df, tmp)
#'
#' # And now reading it - this should print our message.
#' altReadObject(tmp)
#' 
#' # Restoring the old reader:
#' altReadObjectFunction(old)
#'
#' @export
#' @aliases .altLoadObject .loadObject altLoadObject altLoadObjectFunction
altReadObject <- function(...) {
    FUN <- altReadObjectFunction()
    if (is.null(FUN)) {
        FUN <- readObject
    }
    FUN(...)
}

#' @export
#' @rdname altReadObject
altReadObjectFunction <- (function() {
    cur.env <- new.env()
    cur.env$reader <- NULL
    function(fun) {
        prev <- cur.env$reader
        if (missing(fun)) {
            prev
        } else {
            cur.env$reader <- fun
            invisible(prev)
        }
    }
})()

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
altLoadObject <- function(...) {
    FUN <- altLoadObjectFunction()
    if (is.null(FUN)) {
        FUN <- loadObject
    }
    FUN(...)
}

#' @export
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
