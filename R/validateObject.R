#' Validate an object's on-disk representation
#'
#' Validate an object's on-disk representation against the \pkg{takane} specifications.
#' This is done by dispatching to an appropriate validation function based on the type in the \code{OBJECT} file.
#'
#' @param path String containing a path to a directory, itself created with a \code{\link{saveObject}} method.
#' @param metadata List containing metadata for the object.
#' If this is not supplied, it is automatically read from the \code{OBJECT} file inside \code{path}.
#' @param type String specifying the name of type of the object.
#' @param fun 
#' For \code{registerValidateObjectFunction}, a function that accepts \code{path} and \code{metadata}, and raises an error if the object at \code{path} is invalid.
#' It can be assumed that \code{metadata} is a list created by reading \code{OBJECT}.
#'
#' For \code{registerValidateObjectHeightFunction}, a function that accepts \code{path} and \code{metadata}, and returns an integer specifying the \dQuote{height} of the object.
#' This is usually the length for vector-like or 1-dimensional objects, and the extent of the first dimension for higher-dimensional objects.
#' 
#' For \code{registerValidateObjectDimensionsFunction}, a function that accepts \code{path} and \code{metadata}, and returns an integer vector specifying the dimensions of the object.
#'
#' This may also be \code{NULL} to delete an existing registry from any of the functions mentioned above.
#' @param existing Logical scalar indicating the action to take if a function has already been registered for \code{type} -
#' keep the old or new function, or throw an error.
#' 
#' @return 
#' For \code{validateObject}, \code{NULL} is returned invisibly upon success, otherwise an error is raised.
#'
#' For the \code{register*} functions, the supplied \code{fun} is added to the corresponding registry for \code{type}.
#' If \code{fun = NULL}, any existing entry for \code{type} is removed.
#'
#' @seealso
#' \url{https://github.com/ArtifactDB/takane}, for detailed specifications of the on-disk representation for various Bioconductor objects.
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
validateObject <- function(path, metadata=NULL) {
    path <- normalizePath(path, mustWork=TRUE) # protect C code from ~/.
    validate(path, metadata)
    invisible(NULL)
}

#' @export
#' @rdname validateObject
registerValidateObjectFunction <- function(type, fun, existing=c("old", "new", "error")) {
    if (!is.null(fun)) {
        register_validate_function(type, fun, match.arg(existing))
    } else {
        deregister_validate_function(type);
    }
}

#' @export
#' @rdname validateObject
registerValidateObjectHeightFunction <- function(type, fun, existing=c("old", "new", "error")) {
    if (!is.null(fun)) {
        register_height_function(type, fun, match.arg(existing))
    } else {
        deregister_height_function(type);
    }
}

#' @export
#' @rdname validateObject
registerValidateObjectDimensionsFunction <- function(type, fun, existing=c("old", "new", "error")) {
    if (!is.null(fun)) {
        register_dimensions_function(type, fun, match.arg(existing))
    } else {
        deregister_dimensions_function(type);
    }
}
