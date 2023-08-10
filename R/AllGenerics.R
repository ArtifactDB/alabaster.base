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
#' A named list containing the metadata for \code{x} is returned.
#'
#' @details
#' Methods for the \code{stageObject} generic should create a subdirectory at the input \code{path} inside \code{dir}.
#' All files (artifacts and metadata documents) required to represent \code{x} on disk should be created inside \code{path}.
#' Upon method completion, \code{path} should contain:
#' \itemize{
#' \item Zero or one file containing the data inside \code{x}.
#' Methods are free to choose any format and name within \code{path} except for the \code{.json} file extension, 
#' which is reserved for JSON metadata documents (see below).
#' The presence of such a file is optional and may be omitted for metadata-only schemas.
#' \item Zero or many subdirectories containing child objects of \code{x}.
#' Each child object should be saved in its own subdirectory within \code{dir}, 
#' which can have any name that does not conflict with the data file (if present) and does not end with \code{.json}.
#' This allows developers to decompose complex \code{x} into their components for more flexible staging/loading.
#' }
#'
#' The return value of each method should be a named list of metadata, 
#' which will (eventually) be passed to \code{\link{writeMetadata}} to save a JSON metadata file inside the \code{path} subdirectory.
#' This list should contain at least:
#' \itemize{
#' \item \code{$schema}, a string specifying the schema to use to validate the metadata for the class of \code{x}.
#' This may be decorated with the \code{package} attribute to help \code{\link{writeMetadata}} find the package containing the schema.
#' \item \code{path}, a string containing the relative path to the object's file representation inside \code{dir}.
#' For clarity, we will denote the input \code{path} argument as PATHIN and the output \code{path} property as PATHOUT.
#' These are different as PATHIN refers to the directory while PATHOUT refers to a file inside the directory.
#'
#' If a data file exists, PATHOUT should contain the relative path to that file from \code{dir}.
#' Otherwise, for metadata-only schemas, PATHOUT should be set to a relative path of a JSON file inside the PATHIN subdirectory,
#' specifying the location in which the metadata is to be saved by \code{\link{writeMetadata}}.
#' \item \code{is_child}, a logical scalar equal to the input \code{child}. 
#' }
#'
#' This list will usually contain more useful elements to describe \code{x}.
#' The exact nature of those elements will depend on the specified schema for the class of \code{x}.
#'
#' The \code{stageObject} generic will check if PATHIN already exists inside \code{dir} before dispatching to the methods.
#' If so, it will throw an error to ensure that downstream name clashes do not occur.
#' The exception is if PATHIN is \code{"."}, in which case no check is performed; this is useful for eliminating subdirectories in situations where the project contains only one object.
#'
#' @section Saving child objects:
#' The concept of child objects allows developers to break down complex objects into its basic components for convenience.
#' For example, if one \linkS4class{DataFrame} is nested within another as a separate column, the former is a child and the latter is the parent.
#' A list of multiple \linkS4class{DataFrame}s will also represent each DataFrame as a child object.
#' This allows developers to re-use the staging/loading code for DataFrames when reconstructing the complex parent object.
#'
#' If a \code{stageObject} method needs to save a child object, it should do so in a subdirectory of PATHIN (i.e., the input \code{path} argument).
#' This is achieved by calling \code{\link{altStageObject}(child, dir, subpath)} where \code{child} is the child component of \code{x} and \code{subdir} is the desired subdirectory path.
#' Note the period at the start of the function, which ensures that the method respects customizations from alabaster applications (see \code{\link{.altStageObject}} for details).
#' We also suggest creating \code{subdir} with \code{paste0(path, "/", subname)} for a given subdirectory name, which avoids potential problems with non-\code{/} file separators.
#'
#' After creating the child object's subdirectory, the \code{stageObject} method should call \code{\link{writeMetadata}} on the output of \code{altStageObject} to save the child's metadata.
#' This will return a list that can be inserted into the parent's metadata list for the method's return value.
#' All child files created by a \code{stageObject} method should be referenced from the metadata list, 
#' i.e., the child metadata's PATHOUT should be present in in the metadata list as a \code{resource} entry somewhere.
#'
#' Any attempt to use the \code{stageObject} generic to save another non-child object into PATHIN or its subdirectories will cause an error.
#' This ensures that PATHIN contains all and only the contents of \code{x}.
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
#' searchForMethods .searchForMethods
#' @import methods
#' @importFrom jsonlite fromJSON
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
