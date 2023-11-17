#' Move a non-child object in the staging directory
#'
#' \emph{WARNING: this function is deprecated, as directories of non-child objects can just be moved with regular methods (e.g., \code{\link{file.rename}}) in the latest version of \pkg{alabaster}.}
#' Pretty much as it says in the title.
#' This only works with non-child objects as children are referenced by their parents and cannot be safely moved in this manner.
#'
#' @param dir String containing the path to the staging directory.
#' @param from String containing the path to a non-child object inside \code{dir}, as used in \code{\link{acquireMetadata}}.
#' This can also be a redirection to such an object.
#' @param to String containing the new path inside \code{dir}.
#' @param rename.redirections Logical scalar specifying whether redirections pointing to \code{from} should be renamed as \code{to}.
#' 
#' @author Aaron Lun
#'
#' @return
#' The object represented by \code{path} is moved, along with any redirections to it.
#' A \code{NULL} is invisibly returned.
#'
#' @details
#' This function will look around \code{path} for JSON files containing redirections to \code{from}, and update them to point to \code{to}.
#' More specifically, if \code{path} is a subdirectory, it will search in the same directory containing \code{path};
#' otherwise, it will search in the directory containing \code{dirname(path)}.
#' Redirections in other locations will not be removed automatically - these will be caught by \code{\link{checkValidDirectory}} and should be manually updated.
#'
#' If \code{rename.redirections=TRUE}, this function will additionally move the redirection files so that they are named as \code{to}.
#' In the unusual case where \code{from} is the target of multiple redirection files, the renaming process will clobber all of them such that only one of them will be present after the move.
#'
#' @section Safety of moving operations:
#' In general, \pkg{alabaster.*} representations are safe to move as only the parent object's \code{resource.path} metadata properties will contain links to the children's paths.
#' These links are updated with the new \code{to} path after running \code{moveObject} on the parent \code{from}.
#'
#' However, alabaster applications may define custom data structures where the paths are present elsewhere, e.g., in the data file itself or in other metadata properties.
#' If so, applications are reponsible for updating those paths to reflect the naming to \code{to}.
#' 
#' @examples
#' tmp <- tempfile()
#' dir.create(tmp)
#'
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#' meta <- stageObject(df, tmp, path="whee")
#' writeMetadata(meta, tmp)
#'
#' ll <- list(A=1, B=LETTERS, C=DataFrame(X=1:5))
#' meta <- stageObject(ll, tmp, path="stuff")
#' writeMetadata(meta, tmp)
#'
#' redirect <- createRedirection(tmp, "whoop", "whee/simple.csv.gz")
#' writeMetadata(redirect, tmp)
#'
#' list.files(tmp, recursive=TRUE)
#' moveObject(tmp, "whoop", "YAY")
#' list.files(tmp, recursive=TRUE)
#'
#' @export
moveObject <- function(dir, from, to, rename.redirections = TRUE) {
    meta <- acquireMetadata(dir, from)

    if (isTRUE(meta$is_child)) {
        stop("cannot move a child object without moving the parent")
    }

    refpath <- meta$path
    old <- dirname(refpath)
    new.refpath <- .recursive_move(dir, old, to)

    # Searching for redirections.
    searchpath <- file.path(dir, dirname(dirname(refpath)))
    potential <- list.files(searchpath, pattern="\\.json$")

    for (x in potential) {
        redpath <- file.path(searchpath, x)
        meta <- fromJSON(redpath, simplifyVector=FALSE)

        if (dirname(meta[["$schema"]]) == "redirection") {
            survivors <- meta[["redirection"]][["targets"]]
            renamed <- FALSE
            for (i in seq_along(survivors)) {
                x <- survivors[[i]]
                if (x$type == "local" && x$location == refpath) {
                    renamed <- TRUE
                    survivors[[i]]$location <- new.refpath
                    break
                }
            }

            if (renamed) {
                if (rename.redirections) {
                    meta$path <- to
                    unlink(redpath)
                }
                meta[["redirection"]][["targets"]] <- survivors
                writeMetadata(meta, dir)
            }
        }
    }

    invisible(NULL)
}

.replace_path <- function(path, from_slash, to_slash) {
    if (!startsWith(path, from_slash)) {
        NULL
    } else {
        paste0(to_slash, substr(path, nchar(from_slash) + 1, nchar(path)))
    }
}

#' @importFrom jsonlite fromJSON toJSON
.recursive_move <- function(dir, from, to) {
    location <- file.path(dir, from)
    target <- file.path(dir, to)
    dir.create(target, showWarnings=FALSE)
    contents <- list.files(location, all.files=TRUE, no..=TRUE)

    from_slash <- paste0(from, "/")
    to_slash <- paste0(to, "/")
    new.ref <- NULL

    for (x in contents) {
        full <- file.path(location, x)
        if (file.info(full)$isdir) {
            nested.ref <- .recursive_move(dir, paste0(from_slash, x), paste0(to_slash, x))

        } else if (endsWith(x, ".json")) {
            meta <- fromJSON(full, simplifyVector=FALSE)
            meta$path <- .replace_path(meta$path, from_slash, to_slash)
            if (is.null(meta$path)) {
                stop("'path' in metadata is expected to start with '", from_slash, "'")
            }

            updated <- .update_resource_paths(meta, from_slash, to_slash)
            write(file=file.path(target, x), toJSON(updated$metadata, pretty=TRUE, auto_unbox=TRUE, digits=NA))
            new.ref <- meta$path

        } else if (!file.rename(full, file.path(target, x))) {
            stop("failed to rename '", x, "' from '", from, "' to '", to, "'")
        }
    }

    unlink(location, recursive=TRUE)
    new.ref
}

.update_resource_paths <- function(meta, from_slash, to_slash) {
    modified <- FALSE
    replace.resource <- FALSE
    resmeta <- NULL

    if (!is.null(names(meta))) {
        if ("resource" %in% names(meta)) {
            resmeta <- meta$resource
            if ("type" %in% names(resmeta) && resmeta$type == "local") {
                new.path <- .replace_path(resmeta$path, from_slash, to_slash)
                if (!is.null(new.path)) {
                    resmeta$path <- new.path
                    modified <- TRUE
                    replace.resource <- TRUE
                    meta$resource <- FALSE # avoid needless recursion below.
                }
            }
        }
    }

    for (i in seq_along(meta)) {
        if (is.list(meta[[i]])) {
            updated <- .update_resource_paths(meta[[i]], from_slash, to_slash) 
            if (updated$modified) {
                modified <- TRUE
                meta[[i]] <- updated$metadata
            }
        }
    }

    if (replace.resource) {
        meta$resource <- resmeta
    }
    list(modified = modified, metadata = meta)
}
