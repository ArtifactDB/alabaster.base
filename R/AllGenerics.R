#' Save objects to disk
#'
#' Generic to save assorted R objects into appropriate on-disk representations.
#' More methods may be defined by other packages to extend the \pkg{alabaster.base} framework to new classes.
#'
#' @param x A Bioconductor object of the specified class.
#' @param path String containing the path to a directory in which to save \code{x}.
#' @param ... Additional named arguments to pass to specific methods.
#'
#' @return 
#' \code{dir} is created and populated with files containing the contents of \code{x}.
#' \code{NULL} should be invisibly returned.
#'
#' @section Comments for extension developers:
#' Methods for the \code{saveObject} generic should create a directory at \code{path} in which the contents of \code{x} are to be saved.
#' The files may consist of any format, though language-agnostic formats like HDF5, CSV, JSON are preferred.
#' For more complex objects, multiple files and subdirectories may be created within \code{path}. 
#' The only strict requirements are:
#' \itemize{
#' \item There must be an \code{OBJECT} file inside \code{path}, 
#' containing a JSON object with a \code{"type"} string property that specifies the class of the object, e.g., \code{"data_frame"}, \code{"summarized_experiment"}.
#' This will be used by loading functions to determine how to load the files into memory.
#' \item The names of files and subdirectories should not start with \code{_} or \code{.}.
#' These are reserved for applications, e.g., to build manifests or to store additional metadata.
#' }
#'
#' Callers can pass optional parameters to specific \code{saveObject} methods via \code{...}.
#' Any options recognized by a method should be prefixed by the name of the class used in the method's signature,
#' e.g., any options for \code{\link{saveObject,DataFrame-method}} should start with \code{DataFrame.}.
#' This scoping avoids conflicts between otherwise identically-named options of different methods.
#'
#' When developing \code{saveObject} methods of complex objects, a simple approach is to decompose \code{x} into its \dQuote{child} components.
#' Each component can then be saved into a subdirectory of \code{path}, levering the existing \code{saveObject} methods for the component classes.
#' In such cases, extension developers should actually call \code{\link{altSaveObject}} on each child component, rather than calling \link{saveObject} directly.
#' This ensures that any application-level overrides of the loading functions are respected. 
#' It is expected that each method will forward \code{...} (possibly after modification) to any internal \code{\link{altSaveObject}} calls.
#'
#' @section Comments for application developers:
#' Application developers can override \code{saveObject} by specifying a custom function in \code{\link{altSaveObject}}.
#' This can be used to point to a different function to handle the saving process for each class.
#' The custom function can be as simple as a wrapper around \code{saveObject} with some additional actions (e.g., to save more metadata),
#' or may be as complex as a full-fledged generic with its own methods for class-specific customizations.
#'
#' @author Aaron Lun
#' @examples
#' library(S4Vectors)
#' X <- DataFrame(X=LETTERS, Y=sample(3, 26, replace=TRUE))
#' 
#' tmp <- tempfile()
#' saveObject(X, tmp)
#' list.files(tmp, recursive=TRUE)
#'
#' @export
#' @aliases 
#' stageObject stageObject,ANY-method
#' searchForMethods .searchForMethods
#' @import methods
#' @importFrom jsonlite fromJSON
setGeneric("saveObject", function(x, path, ...) {
    if (file.exists(path)) {
        stop("cannot save ", class(x)[1], " at existing path '", path, "'")
    }

    # Need to search here to pick up any subclasses that might have better
    # saveObject methods in yet-to-be-loaded packages.
    if (.search_methods(x)) {
        fun <- selectMethod("saveObject", class(x)[1], optional=TRUE)
        if (!is.null(fun)) {
            return(fun(x, path, ...))
        }
    }

    # Only validate once we're outside of nested calls, as each parent should
    # validate its children; separate validation of each child is redundant.
    do_validate <- !is_nested$status
    if (do_validate) {
        is_nested$status <- TRUE
        on.exit({ is_nested$status <- FALSE }, add=TRUE, after=FALSE)
    }

    standardGeneric("saveObject")

    if (do_validate) {
        validateObject(path)
    }
})

is_nested <- new.env()
is_nested$status <- FALSE

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
setGeneric("stageObject", function(x, dir, path, child=FALSE, ...) {
    if (path != "." && file.exists(full.path <- file.path(dir, path))) {
        stop("cannot stage ", class(x)[1], " at existing path '", full.path, "'")
    }
    if (grepl("\\\\", path)) {
        stop("Windows-style path separators are not allowed")
    }

    if (!child) {
        parent <- dirname(path)
        while (parent != ".") {
            ppath <- file.path(dir, parent)
            candidates <- list.files(ppath, pattern="\\.json$")
            for (can in candidates) {
                schema <- fromJSON(file.path(ppath, can), simplifyVector=FALSE)[["$schema"]]
                if (!startsWith(schema, "redirection/")) {
                    stop("cannot save a non-child object inside another object's subdirectory at '", parent, "'")
                }
            }
            parent <- dirname(parent)
        }
    }

    # Need to search here to pick up any subclasses that might have better
    # stageObject methods in yet-to-be-loaded packages.
    if (.search_methods(x)) {
        fun <- selectMethod("stageObject", class(x)[1], optional=TRUE)
        if (!is.null(fun)) {
            return(fun(x, dir, path, child=child, ...))
        }
    }

    standardGeneric("stageObject")
})

#' Acquire file or metadata
#'
#' \emph{WARNING: these functions are deprecated. 
#' Applications are expected to handle acquisition of files before loaders are called.}
#' Acquire a file or metadata for loading.
#' As one might expect, these are typically used inside a \code{load*} function.
#'
#' @param project Any value specifying the project of interest.
#' The default methods expect a string containing a path to a staging directory,
#' but other objects can be used to control dispatch.
#' @param path String containing a relative path to a resource inside the staging directory.
#'
#' @return
#' \code{acquireFile} methods return a local path to the file corresponding to the requested resource.
#'
#' \code{acquireMetadata} methods return a named list of metadata for the requested resource.
#' 
#' @details
#' By default, files and metadata are loaded from the same staging directory that is written to by \code{\link{stageObject}}.
#' alabaster applications can define custom methods to obtain the files and metadata from a different location, e.g., remote databases.
#' This is achieved by dispatching on a different class of \code{project}.
#'
#' Each custom acquisition method should take two arguments.
#' The first argument is an R object representing some concept of a \dQuote{project}.
#' In the default case, this is a string containing a path to the staging directory representing the project.
#' However, it can be anything, e.g., a number containing a database identifier, a list of identifiers and versions, and so on -
#' as long as the custom acquisition method is capable of understanding it, the \code{load*} functions don't care.
#'
#' The second argument is a string containing the relative path to the resource inside that project.
#' This should be the path to a specific file inside the project, not the subdirectory containing the file.
#' More concretely, it should be equivalent to the \code{path} in the \emph{output} of \code{\link{stageObject}},
#' not the path to the subdirectory used as the input to the same function.
#'
#' The return value for each custom acquisition function should be the same as their local counterparts.
#' That is, any custom file acquisition function should return a file path,
#' and any custom metadata acquisition function should return a naamed list of metadata.
#'
#' @author Aaron Lun
#' @examples
#' # Staging an example DataFrame:
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#' tmp <- tempfile()
#' dir.create(tmp)
#' info <- stageObject(df, tmp, path="coldata")
#' writeMetadata(info, tmp)
#'
#' # Retrieving the metadata:
#' meta <- acquireMetadata(tmp, "coldata/simple.csv.gz")
#' str(meta)
#'
#' # Retrieving the file:
#' acquireFile(tmp, "coldata/simple.csv.gz")
#'
#' @export
#' @name acquireFile
setGeneric("acquireFile", function(project, path) standardGeneric("acquireFile"))

#' @export
#' @rdname acquireFile
setGeneric("acquireMetadata", function(project, path) {
    ans <- standardGeneric("acquireMetadata")

    # Handling redirections. Applications that want to support different redirection
    # types should implement the relevant logic in their acquireMetadata method.
    if (dirname(ans[["$schema"]]) == "redirection") {
        first <- ans[["redirection"]][["targets"]][[1]]
        if (first$type != "local") {
            stop("unknown redirection type '", first$type, "' to '", first$location, "'")
        }
        ans <- acquireMetadata(project, first$location)
    }

    ans
})
