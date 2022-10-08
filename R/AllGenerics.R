#' Stage assorted objects
#'
#' Generic to stage assorted R objects.
#' More methods may be defined by other packages to extend the \pkg{alabaster.base} framework to new classes.
#'
#' @param x A Bioconductor object of the specified class.
#' @param dir String containing the path to the staging directory.
#' @param path String containing a prefix of the relative path inside \code{dir} where \code{x} is to be saved.
#' The actual path used to save \code{x} may include additional components, see Details.
#' @param child Logical scalar indicating whether \code{x} is a child of a larger object.
#' @param ... Further arguments to pass to specific methods.
#'
#' @return 
#' \code{dir} is populated with files containing the contents of \code{x}.
#' All created files should be prefixed by \code{path}, either in the file name or as part of a subdirectory path.
#'
#' A named list containing the metadata for \code{x} is returned, containing at least:
#' \itemize{
#' \item \code{$schema}, a string specifying the schema to use to validate the metadata.
#' This may be decorated with the \code{package} attribute to help \code{\link{.writeMetadata}} find the package containing the schema.
#' \item \code{path}, a string containing the path to some file prefixed by the input \code{path}.
#' \item \code{is_child}, a logical scalar equal to the input \code{child}. 
#' }
#' All files created by a \code{stageObject} method should be referenced from the metadata list, directly or otherwise (e.g., via child resources).
#'
#' @details
#' All methods are expected to create a subdirectory at the input \code{path}.
#' All artifacts required to represent \code{x} should be created inside this subdirectory.
#' Each artifact is associated with its own JSON-formatted metadata file and is processed by its own staging and loading methods.
#' This makes it easy to decompose complex objects into manageable components.
#'
#' Exactly one artifact in this subdirectory should be marked \code{is_child = FALSE} and should reference (indirectly or otherwise) all other artifacts with \code{is_child = TRUE}.
#' The non-child artifact is considered the \dQuote{entrypoint} file that should be referenced by \code{loadObject} to restore \code{x} in memory.
#' Keep in mind that the relative path of the entrypoint file will differ from the input \code{path} directory;
#' if the former is needed (e.g., to reference \code{x} from the metadata of a larger object), use the \code{path} string returned in the output list.
#'
#' If a method for this generic needs to stage child artifacts, it should call \code{\link{.stageObject}} rather than \code{stageObject} (note the period at the start of the former).
#' This ensures that the staging method will respect customizations from alabaster applications that define their own generic in \code{\link{.altStageObject}}.
#' Child objects of \code{x} should be saved in the same subdirectory as \code{path};
#' any attempt to save another non-child object into \code{path} will cause an error.
#'
#' The \code{stageObject} generic will check if the \code{path} already exists before dispatching to the methods.
#' If so, it will throw an error to ensure that downstream name clashes do not occur.
#' The exception is if \code{path = "."}, in which case no check is performed; this is useful for eliminating subdirectories in situations where the project contains only one object.
#' 
#' @author Aaron Lun
#' @examples
#' tmp <- tempfile()
#' dir.create(tmp)
#' 
#' library(S4Vectors)
#' X <- DataFrame(X=LETTERS, Y=sample(3, 26, replace=TRUE))
#' stageObject(X, tmp, path="test1")
#' list.files(file.path(tmp, "test1"))
#'
#' @export
#' @aliases stageObject,ANY-method
#' .searchForMethods
#' @import methods
#' @importFrom jsonlite fromJSON
setGeneric("stageObject", function(x, dir, path, child=FALSE, ...) {
    if (path != "." && file.exists(full.path <- file.path(dir, path))) {
        stop("cannot stage ", class(x)[1], " at existing path '", full.path, "'")
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

    .searchMethods(x)

    standardGeneric("stageObject")
})

#' Acquire file or metadata
#'
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
#' Artificer applications can define custom methods to obtain the files and metadata from a different location, e.g., remote databases.
#' This is achieved by dispatching on a different value of \code{project}.
#'
#' Each custom acquisition method should take two arguments.
#' The first argument is an R object representing some concept of a \dQuote{project}.
#' In the default case, this is a string containing a path to the staging directory representing the project.
#' However, it can be anything, e.g., a number containing a database identifier, a list of identifiers and versions, and so on -
#' as long as the custom acquisition method is capable of understanding it, the \code{load*} functions don't care.
#' The second argument is a string containing the relative path to the resource inside that project.
#'
#' The return value for each custom acquisition function should be the same as their local counterparts.
#' That is, any custom file acquisition function should return a file path,
#' and any custom metadata acquisition function should return a naamed list of metadata.
#'
#' @author Aaron Lun
#'
#' @export
#' @name acquireFile
setGeneric("acquireFile", function(project, path) standardGeneric("acquireFile"))

#' @export
#' @rdname acquireFile
setGeneric("acquireMetadata", function(project, path) standardGeneric("acquireMetadata"))
