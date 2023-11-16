#' Read an object from disk
#'
#' Read an object from its on-disk representation.
#' This is done dispatching to an appropriate loading function based on the type in the \code{OBJECT} file.
#'
#' @param path String containing a path to a directory, itself created with a \code{\link{saveObject}} method.
#' @param ... Further arguments to pass to individual methods.
#' @param type String specifying the name of type to register.
#' @param fun A loading function that accepts \code{path} and \code{...}, and returns the associated object.
#' This may also be \code{NULL} to delete an existing registry.
#' 
#' @return 
#' For \code{readObject}, an object created from the on-disk representation in \code{path}.
#'
#' For \code{readObjectFunctionRegistry}, a named list of functions used to load each object type.
#'
#' For \code{registerReadObjectFunction}, the function is added to the registry.
#'
#' @section Comments for extension developers:
#' When writing alabaster extensions, developers may need to load child objects inside the loading functions for their classes. 
#' In such cases, developers should use \code{\link{altRoadObject}} rather than calling \code{readObject} directly.
#' This ensures that any application-level overrides of the loading functions are respected. 
#' Once in memory, the child objects can then be assembled into more complex objects by the developer's loading function.
#'
#' \code{readObject} uses an internal registry of functions to decide how an object should be loaded into memory.
#' Developers of alabaster extensions can add extra functions to this registry, usually in the \code{\link{.onLoad}} function of their packages.
#' Alternatively, extension developers can request the addition of their packages to default registry.
#'
#' If a loading function makes use of additional arguments, it should be scoped by the name of the class for each method, e.g., \code{list.parallel}, \code{dataframe.include.nested}.
#' This avoids problems with conflicts in the interpretation of identically named arguments between different functions.
#' It is expected that arguments in \code{...} are forwarded to internal \code{\link{altReadObject}} calls.
#'
#' @section Comments for application developers:
#' Application developers can override the behavior of \code{readObject} by specifying a custom function in \code{\link{altReadObject}}.
#' This is typically used to point to a different set of application-specific schemas, 
#' which in turn point to (potentially custom) reading functions in their \code{_application.restore.R} properties.
#' In most applications, the override should be defined with \code{customreadObjectHelper}, which simplifies the process of specifying a different set of schemas.
#'
#' @author Aaron Lun
#' @examples
#' # Same example as stageObject, but reversed.
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#'
#' tmp <- tempfile()
#' out <- stageObject(df, tmp)
#' readObject(out, tmp)
#' 
#' @export
#' @aliases loadObject schemaLocations customLoadObjectHelper .loadObjectInternal 
readObject <- function(path, ...) {
    type <- readLines(file.path(path, "OBJECT"))
    meth <- registry$registry[[type]]
    if (is.null(meth)) {
        stop("cannot read unknown object type '", type, "'")
    } else if (is.character(meth)) {
        meth <- eval(parse(text=meth))
        registry$registry[[type]] <- meth
    }
    meth(path, ...)
}

registry <- new.env()
registry$registry <- list(
    atomic_vector="alabaster.base::readAtomicVector",
    hdf5_simple_list="alabaster.base::readBaseList",
    json_simple_list="alabaster.base::readBaseList",
    string_factor="alabaster.base::readBaseFactor",
    data_frame="alabaster.base::readDataFrame",
    data_frame_factor="alabaster.base::readDataFrameFactor"
)

#' @export
#' @rdname readObject
readObjectFunctionRegistry <- function() {
    registry$registry
}

#' @export
#' @rdname readObject
registerReadObjectFunction <- function(type, fun) {
    if (!is.null(fun) && !is.null(registry$registry[[type]])) {
        warning("readObject function has already been registered for object type '", type, "'")
    }
    registry$registry[[type]] <- fun
}


#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
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
#' @importFrom jsonlite fromJSON
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

# Soft-deprecated backwards compatibility fixes.

#' @export
.loadObjectInternal <- function(...) customloadObjectHelper(...)
