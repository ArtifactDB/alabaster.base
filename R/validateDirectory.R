#' Validate a directory of objects
#'
#' Check whether each object in a directory is valid by calling \code{\link{validateObject}} on each non-nested object.
#'
#' @param dir String containing the path to a directory with subdirectories populated by \code{\link{saveObject}}.
#' @param legacy Logical scalar indicating whether to validate a directory with legacy objects (created by the old \code{stageObject}).
#' If \code{NULL}, this is auto-detected from the contents of \code{dir}.
#' @param ... Further arguments to use when \code{legacy=TRUE}, for back-compatibility only.
#'
#' @return Character vector of the paths inside \code{dir} that were validated, invisibly.
#' If any validation failed, an error is raised.
#'
#' @details
#' We assume that the process of validating an object will call \code{\link{validateObject}} on any nested objects.
#' This allows us to skip explicit calls to \code{\link{validateObject}} on each component of a complex object. 
#'
#' @author Aaron Lun
#' @examples
#' # Mocking up an object:
#' library(S4Vectors)
#' ncols <- 123
#' df <- DataFrame(
#'     X = rep(LETTERS[1:3], length.out=ncols),
#'     Y = runif(ncols)
#' )
#' df$Z <- DataFrame(AA = sample(ncols))
#'
#' # Mocking up the directory:
#' tmp <- tempfile()
#' dir.create(tmp, recursive=TRUE)
#' saveObject(df, file.path(tmp, "foo"))
#' 
#' # Checking that it's valid:
#' validateDirectory(tmp)
#'
#' # Adding an invalid object:
#' dir.create(file.path(tmp, "bar"))
#' write(file=file.path(tmp, "bar", "OBJECT"), '[ "WHEEE" ]')
#' try(validateDirectory(tmp))
#'
#' @aliases checkValidDirectory
#' @export
validateDirectory <- function(dir, legacy=NULL, ...) {
    if (is.null(legacy)) {
        legacy <- length(list.files(dir, recursive=TRUE, pattern="^OBJECT$")) == 0L
    }

    if (!legacy) {
        objects <- listObjects(dir)
        for (x in objects$path) {
            tryCatch(validateObject(file.path(dir, x)), error=function(e) stop("failed to validate '", x, "'; ", e$message))
        }
        invisible(objects$path)
    } else {
        legacy.validateDirectory(dir, ...)
    }
}

legacy.validateDirectory <- function(dir, validate.metadata = TRUE, schema.locations = NULL, attempt.load = FALSE) {
    all.files <- list.files(dir, recursive=TRUE)
    is.json <- endsWith(all.files, ".json")
    meta.files <- all.files[is.json]
    other.files <- all.files[!is.json]

    schema.paths <- list()
    if (is.null(schema.locations)) {
        schema.locations <- .default_schema_locations()
    }

    # Opening all the metadata files.
    am.child <- character(0)
    not.child <- character(0)
    expected.child <- character(0)
    redirects <- character(0)

    for (metapath in meta.files) {
        jpath <- file.path(dir, metapath)
        meta <- fromJSON(jpath, simplifyVector=TRUE, simplifyMatrix=FALSE, simplifyDataFrame=FALSE)

        schema.id <- meta[["$schema"]]
        if (validate.metadata) {
            if (!(schema.id %in% schema.paths)) {
                schema.path <- .hunt_for_schemas(meta[["$schema"]], schema.locations)
                schema.paths[[schema.id]] <- schema.path
            } 
            schema.path <- schema.paths[[schema.id]]

            tryCatch(
                jsonvalidate::json_validate(jpath, schema.path, error=TRUE, engine="ajv"), 
                error=function(e) {
                    stop("failed to validate metadata at '", jpath, "'\n  - ", e$message)
                }
            )
        }

        # Special case for redirections
        if (startsWith(schema.id, "redirection/")) {
            if (paste0(meta$path, ".json") != metapath) {
                stop("metadata in '", metapath, "' references an unexpected path '", meta$path, "'")
            }
            if (meta$path %in% all.files) {
                stop("metadata in '", metapath, "' contains a redirection from existing path '", meta$path, "'")
            }
            for (target in meta[["redirection"]][["targets"]]) {
                if (target$type == "local") {
                    redirects <- c(redirects, target$location)
                }
            }
            next
        }

        # Checking the path.
        if (!(meta$path %in% all.files)) {
            stop("metadata in '", metapath, "' references a non-existent path '", meta$path, "'")
        }
        if (meta$path != metapath && paste0(meta$path, ".json") != metapath) {
            stop("metadata in '", metapath, "' references an unexpected path '", meta$path, "'")
        }

        if (!isTRUE(meta$is_child)) {
            not.child <- c(not.child, meta$path)
            if (attempt.load) {
                tryCatch(
                    altLoadObject(meta, dir),
                    error=function(e) {
                        stop("failed to load non-child object at '", meta$path, "'\n  - ", e$message)
                    }
                )
            }
        } else {
            am.child <- c(am.child, meta$path)
        }

        # Checking that all children live in subdirectories of metapath's directory, not at the top level.
        children <- .fetch_resource_paths(meta)
        subdir.prefix <- paste0(dirname(metapath), "/")
        if (any(nonnest <- !startsWith(dirname(children), subdir.prefix))) { 
            stop("metadata in '", metapath, "' references non-nested child '", children[nonnest][1], "'") 
        }

        expected.child <- c(expected.child, children)
    }

    # Checking that all children are not marked as non-children.
    conflicts <- intersect(expected.child, not.child)
    if (length(conflicts)) {
        stop("non-child object in '", conflicts[1], "' is referenced by another object")
    }

    if (dup <- anyDuplicated(expected.child)) {
        stop("multiple references to child at '", expected.child[dup], "'")
    }

    missing.child <- setdiff(expected.child, am.child)
    if (length(missing.child)) {
        stop("missing child object '", missing.child[1], "'")
    }

    extra.child <- setdiff(am.child, expected.child)
    if (length(extra.child)) {
        stop("non-referenced child object in '", extra.child[1], "'")
    }

    extra.other <- setdiff(other.files, c(am.child, not.child))
    if (length(extra.other)) {
        stop("unknown file at '", extra.other[1], "'")
    }

    # Checking that non-children are not nested within each other.
    safe.set <- "."
    offenders <- sort(dirname(not.child))
    for (i in seq_along(offenders)) {
        if (i > 1 && startsWith(offenders[i], paste0(offenders[i-1], "/"))) {
            stop("non-child object at '", offenders[i], "' is nested inside the directory of '", offenders[i-1], "'")
        }
    }

    # Checking that redirects are valid.
    dangling.direct <- !(redirects %in% am.child) & !(redirects %in% not.child)
    if (any(dangling.direct)) {
        stop("invalid redirection to '", dangling.direct[1], "'")
    }
}

.fetch_resource_paths <- function(x) {
    if (!is.list(x)) {
        return(NULL)
    }

    if (!is.null(names(x))) {
        is.res <- which(names(x) == "resource")
        if (length(is.res)) {
            return(x[[is.res[1]]]$path)
        } 
    }

    collected <- character(0)
    for (i in seq_along(x)) {
        collected <- c(collected, .fetch_resource_paths(x[[i]]))
    }
    collected
}

# Soft-deprecated back-compatibility fixes.

#' @export
checkValidDirectory <- function(...) validateDirectory(...)
