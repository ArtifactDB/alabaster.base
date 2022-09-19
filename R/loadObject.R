#' Load an object from its metadata
#'
#' Load an object from its metadata, based on the loading function specified in its schema.
#'
#' @param info Named list containing the metadata for this object.
#' @param project Any argument accepted by the acquisition functions, see \code{?\link{acquireFile}}.
#' By default, this should be a string containing the path to a staging directory.
#' @param ... Further arguments to pass to the specific loading function listed in the schema.
#' 
#' @return An object corresponding to \code{info}, as defined by the loading function.
#'
#' @details
#' The \code{loadObject} function is intended to load objects where the class is not known in advance.
#' Most typically, this is indirectly called inside other loading functions to restore \emph{child} objects of arbitrary type.
#' Once in memory, the child objects can then be assembled into more complex objects by the caller.
#' (It would be unwise to use \code{loadObject} to try restore a non-child object as this would result in infinite recursion.)
#' 
#' This function will look through the schemas in \pkg{alabaster.schemas} to find the schema specified in \code{info$`$schema`}.
#' Upon discovery, \code{loadObject} will extract the loading function from the \code{_attributes.restore.R} property of the schema;
#' this should be a string that contains a namespaced function, which can be parsed and evaluated to obtain said function.
#' \code{loadObject} will then call the loading function with the supplied arguments.
#' Developers can temporarily add extra packages to the schema search path by supplying package names in the \code{alabaster.schema.locations} option;
#' schema files are expected to be stored in the \code{schemas} subdirectory of each package's installation directory. 
#'
#' Developers of Artificer extensions should use \code{\link{.loadObject}} rather than calling \code{\link{loadObject}} directly.
#' This ensures that any application-level overrides of the loading functions are respected. 
#' Developers of Artificer applications should also read the commentary in \code{?"\link{.altLoadObject}"}.
#'
#' @author Aaron Lun
#' @examples
#' # Same example as stageObject, but reversed.
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#'
#' # First staging it:
#' tmp <- tempfile()
#' dir.create(tmp)
#' out <- stageObject(df, tmp, path="coldata")
#'
#' # Loading it:
#' loadObject(out, tmp)
#' 
#' @export
#' @aliases .loadObjectInternal
#' @aliases schemaLocations
#' @importFrom jsonlite fromJSON
loadObject <- function(info, project, ...) {
    .loadObjectInternal(info, 
        project, 
        ...,
        .locations=c(getOption("alabaster.schema.locations"), "alabaster.schemas"),
        .memory=restore.memory
    )
}

restore.memory <- new.env()
restore.memory$cache <- list()

#' @export
#' @import alabaster.schemas
.loadObjectInternal <- function(info, project, ..., .locations, .memory, .fallback=NULL) {
    schema <- info[["$schema"]]

    if (is.null(FUN <- .memory$cache[[schema]])) {
        # Hunt for our schemas!
        schema.path <- "" 
        for (pkg in .locations) {
            schema.path <- system.file("schemas", schema, package=pkg)
            if (schema.path != "") {
                break
            }
        }

        if (schema.path == "") {
            if (is.null(.fallback)) {
                stop("couldn't find the '", schema, "' schema")
            }
            schema.path <- .fallback(schema)
        }

        schema.data <- fromJSON(schema.path, simplifyVector=TRUE, simplifyMatrix=FALSE, simplifyDataFrame=FALSE)
        restore <- schema.data[["_attributes"]][["restore"]][["R"]]
        if (is.null(restore)) {
            stop("could not find an R context to restore '", schema, "'")
        }

        FUN <- eval(parse(text=restore))
        .memory$cache[[schema]] <- FUN
    }

    FUN(info, project, ...)
}

#' @export
schemaLocations <- function() {
    .Deprecated()
    c(getOption("alabaster.schema.locations"), "alabaster.schemas")
}
