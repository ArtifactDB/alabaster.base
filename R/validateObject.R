#' Validate an object's on-disk representation
#'
#' Validate an object's on-disk representation.
#' This is done by dispatching to an appropriate validation function based on the type in the \code{OBJECT} file.
#'
#' @param path String containing a path to a directory, itself created with a \code{\link{saveObject}} method.
#' @param type String specifying the name of type of the object.
#' If this is not supplied for \code{validateObject}, it is automatically determined from the \code{OBJECT} file in \code{path}.
#' @param fun 
#' For \code{registerValidateObjectFunction}, a function that accepts \code{path} and raises an error if the object at \code{path} is invalid.
#'
#' For \code{registerValidateObjectHeightFunction}, a function that accepts \code{path} and returns an integer specifying the \dQuote{height} of the object.
#' This is usually the length for vector-like or 1-dimensional objects, and the extent of the first dimension for higher-dimensional objects.
#' 
#' This may also be \code{NULL} to delete an existing registry.
#' 
#' @return 
#' For \code{validateObject}, \code{NULL} is returned invisibly upon success, otherwise an error is raised.
#'
#' For \code{registerValidateObjectFunction}, the function is added to the registry for \code{type}.
#' If \code{fun = NULL}, any existing entry for \code{type} is removed.
#'
#' For \code{registerValidateObjectHeightFunction}, the function is added to the registry for \code{type}.
#' If \code{fun = NULL}, any existing entry for \code{type} is removed.
#'
#' @author Aaron Lun
#' @examples
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#'
#' tmp <- tempfile()
#' saveObject(df, tmp)
#' validateObject(tmp)
#' 
#' @export
validateObject <- function(path, type=NULL) {
    if (is.null(type)) {
        type <- readLines(file.path(path, "OBJECT"))
    }
    validate(path, type)
    invisible(NULL)
}

#' @export
#' @rdname validateObject
registerValidateObjectFunction <- function(type, fun) {
    if (!is.null(fun)) {
        register_validate_function(type, fun)
    } else {
        deregister_validate_function(type);
    }
}

#' @export
#' @rdname validateObject
registerValidateObjectHeightFunction <- function(type, fun) {
    if (!is.null(fun)) {
        register_height_function(type, fun)
    } else {
        deregister_height_function(type);
    }
}
