#' Load an object from its metadata
#'
#' Load an object from its metadata, based on the loading function specified in its schema.
#'
#' @param info Named list containing the metadata for this object.
#' @param project Any argument accepted by the acquisition functions, see \code{?\link{acquireFile}}.
#' By default, this should be a string containing the path to a staging directory.
#' @param ... Further arguments to pass to the specific loading function listed in the schema.
#' @param .locations Character vector of package names containing application-specific schemas.
#' @param .memory An environment used to cache the loading functions, to avoid extra schema file reads on subsequent calls.
#' @param .fallback Function that accepts a schema string (e.g., \code{"data_frame/v1.json"}) and returns the path to a schema.
#' If \code{NULL}, no fallback is used and an error is raised if the schema cannot be found.
#' 
#' @return An object corresponding to \code{info}, as defined by the loading function.
#'
#' @details
#' The \code{loadObject} function loads an object from file into memory based on the schema specified in \code{info},
#' effectively reversing the activity of the corresponding \code{\link{stageObject}} method.
#' It does so by extracting the name of the appropriate loading function from the \code{_attributes.restore.R} property of the schema;
#' this should be a string that contains a namespaced function, which can be parsed and evaluated to obtain said function.
#' \code{loadObject} will then call the loading function with the supplied arguments.
#'
#' @section Comments for extension developers:
#' When writing alabaster extensions, developers may need to load child objects inside the loading functions for their classes. 
#' In such cases, developers should use \code{\link{.loadObject}} rather than calling \code{\link{loadObject}} directly.
#' This ensures that any application-level overrides of the loading functions are respected. 
#' Once in memory, the child objects can then be assembled into more complex objects by the developer's loading function.
#'
#' By default, \code{loadObject} will look through the schemas in \pkg{alabaster.schemas} to find the schema specified in \code{info$`$schema`}.
#' Developers of alabaster extensions can temporarily add extra packages to the schema search path by supplying package names in the \code{alabaster.schema.locations} option;
#' schema files are expected to be stored in the \code{schemas} subdirectory of each package's installation directory. 
#' In the long term, extension developers should request the addition of their packages to \code{loadObject}'s default search path.
#'
#' @section Comments for application developers:
#' Application developers can override the behavior of \code{loadObject} by specifying a custom function in \code{\link{.altLoadObject}}.
#' This is typically used to point to a different set of application-specific schemas, 
#' which in turn point to (potentially custom) loading functions in their \code{_application.restore.R} properties.
#' In most applications, the override should be defined with \code{customloadObjectHelper}, which simplifies the process of specifying a different set of schemas.
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
    customloadObjectHelper(info, 
        project, 
        ...,
        .locations=.default_schema_locations(),
        .memory=restore.memory
    )
}

restore.memory <- new.env()
restore.memory$cache <- list()

#' @export
.loadObjectInternal <- function(...) {
    .Deprecated(new=".loadObjectHelper")
    customloadObjectHelper(...)
}

#' @export
#' @rdname loadObject
customloadObjectHelper <- function(info, project, ..., .locations, .memory, .fallback=NULL) {
    schema <- info[["$schema"]]

    if (is.null(FUN <- .memory$cache[[schema]])) {
        schema.path <- .hunt_for_schemas(schema, .locations, .fallback=.fallback)

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
    .Defunct()
}

#' @import alabaster.schemas
.default_schema_locations <- function() c(getOption("alabaster.schema.locations"), "alabaster.schemas")

.hunt_for_schemas <- function(schema, .locations, .fallback=NULL) {
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

    schema.path
}
