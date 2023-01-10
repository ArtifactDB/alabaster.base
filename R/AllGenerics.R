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
#' Methods for the \code{stageObject} generic are expected to create a subdirectory at the input \code{path} inside \code{dir}.
#' All files (artifacts and metadata documents) required to represent \code{x} on disk should be created inside \code{path}.
#' The expected contents of \code{path} are:
#' \itemize{
#' \item Exactly one JSON metadata document with a name ending in \code{.json}.
#' This contains the metadata describing \code{x}, according to the schema defined in the \code{$schema} property.
#' If a data file exists (see next), the \code{path} property should contain the relative path to that file from \code{dir};
#' otherwise it should contain the relative path of the metadata document itself.
#' \item (Optional) A file containing the data inside \code{x}.
#' This file should have the same name as the metadata file after stripping the \code{.json} extension.
#' Methods are free to choose any format and name within \code{path} except for the \code{.json} file extension, 
#' which is reserved for metadata documents created with \code{\link{.writeMetadata}}.
#' \item (Optional) Further subdirectories containing child objects of \code{x}.
#' Each child object should be saved in its own subdirectory, and should be referenced via a \code{resource} object within the metadata for \code{x}.
#' When saving children, methods should call \code{\link{.stageObject}} rather than \code{stageObject} (note the period at the start of the former).
#' This ensures that the staging method will respect customizations from alabaster applications that define their own generic in \code{\link{.altStageObject}}.
#' }
#' 
#' Methods can create both a data file and multiple subdirectories.
#' In this manner, we can decompose complex \code{x} into their components for easier handling.
#' 
#' Keep in mind that the returned \code{path} will differ from the input \code{path};
#' the latter refers to the directory while the former refers to a file inside the directory.
#' Methods should use the former to reference \code{x} from the metadata of a parent object.
#'
#' The \code{stageObject} generic will check if the \code{path} already exists before dispatching to the methods.
#' If so, it will throw an error to ensure that downstream name clashes do not occur.
#' The exception is if \code{path = "."}, in which case no check is performed; this is useful for eliminating subdirectories in situations where the project contains only one object.
#'
#' Any attempt to use the \code{stageObject} generic to save another non-child object into \code{path} or its subdirectories will cause an error.
#' This ensures that \code{path} contains all and only the contents of \code{x}.
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
#' @seealso
#' \code{\link{checkValidDirectory}}, for validation of the staged contents.
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
#' @examples
#' # Staging an example DataFrame:
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#' tmp <- tempfile()
#' dir.create(tmp)
#' info <- stageObject(df, tmp, path="coldata")
#' .writeMetadata(info, tmp)
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
