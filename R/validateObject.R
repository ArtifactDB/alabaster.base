#' Validate an object's on-disk representation
#'
#' Validate an object's on-disk representation.
#' This is done by dispatching to an appropriate validation function based on the type in the \code{OBJECT} file.
#'
#' @param path String containing a path to a directory, itself created with a \code{\link{saveObject}} method.
#' @param type String specifying the name of type of the object.
#' If this is not supplied for \code{readObject}, it is automatically determined from the \code{OBJECT} file in \code{path}.
#' @param fun A validation function that accepts \code{path} and \code{...}, and returns the associated object.
#' This may also be \code{NULL} to delete an existing registry.
#' 
#' @return 
#' For \code{validateObject}, \code{NULL} is returned invisibly upon success, otherwise an error is raised.
#'
#' For \code{validateObjectFunctionRegistry}, a named list of functions used to load each object type.
#'
#' For \code{registerValidateObjectFunction}, the function is added to the registry.
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
validateObject <- function(path, type=NULL, ...) {
    if (is.null(type)) {
        type <- readLines(file.path(path, "OBJECT"))
    }

    meth <- validate.registry$registry[[type]]
    if (is.null(meth)) {
        stop("cannot validate unknown object type '", type, "'")
    } else if (is.character(meth)) {
        meth <- eval(parse(text=meth))
        validate.registry$registry[[type]] <- meth
    }
    meth(path, ...)
}

validate.registry <- new.env()
validate.registry$registry <- list(
    atomic_vector="alabaster.base::validateAtomicVector",
    string_factor="alabaster.base::validateBaseFactor",
    simple_list="alabaster.base::validateBaseList",
    data_frame="alabaster.base::validateDataFrame",
    data_frame_factor="alabaster.base::validateDataFrameFactor"
)

#' @export
#' @rdname validateObject
validateObjectFunctionRegistry <- function() {
    validate.registry$registry
}

#' @export
#' @rdname validateObject
registerValidateObjectFunction <- function(type, fun) {
    if (!is.null(fun) && !is.null(validate.registry$registry[[type]])) {
        warning("validateObject function has alvalidatey been registered for object type '", type, "'")
    }
    validate.registry$registry[[type]] <- fun
}
