#' Check if a staging directory is valid
#'
#' Check whether a staging directory is valid in terms of its structure and metadata.
#'
#' @param dir String containing the path to a staging directory.
#'
#' @return \code{NULL} invisibly on success, otherwise an error is raised.
#'
#' @details
#' This function verifies that the restrictions described in \code{\link{stageObject}} are respected, namely:
#' \itemize{
#' \item Each object is represented by subdirectory with a single JSON document.
#' \item Each JSON metadata document's \code{path} property exists and is consistent with the path of the document itself.
#' \item Child objects are nested in subdirectories of the parent object's directory.
#' \item Child objects have the \code{is_child} property set to true in their metadata.
#' \item Each child object is referenced exactly once in its parent object's metadata.
#' }
#'
#' This function will also check that redirections are valid:
#' \itemize{
#' \item The \code{path} property of the redirection does \emph{not} exist and is consistent with the path of the redirection document.
#' \item The redirection target location exists in the directory.
#' }
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
#' .writeMetadata(stageObject(df, tmp, "foo"), tmp)
#' 
#' # Checking that it's valid:
#' checkValidDirectory(tmp)
#' @export
#' @importFrom jsonlite fromJSON
checkValidDirectory <- function(dir) {
    all.files <- list.files(dir, recursive=TRUE)
    is.json <- endsWith(all.files, ".json")
    meta.files <- all.files[is.json]
    other.files <- all.files[!is.json]

    # Opening all the metadata files.
    am.child <- character(0)
    not.child <- character(0)
    expected.child <- character(0)
    redirects <- character(0)

    for (metapath in meta.files) {
        meta <- fromJSON(file.path(dir, metapath), simplifyVector=TRUE, simplifyMatrix=FALSE, simplifyDataFrame=FALSE)

        # Special case for redirections
        if (startsWith(meta[["$schema"]], "redirection/")) {
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
