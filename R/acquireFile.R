#' @export
#' @rdname acquireFile
setMethod("acquireFile", "character", function(project, path) file.path(project, path))

#' @export
#' @rdname acquireFile
#' @importFrom jsonlite fromJSON
setMethod("acquireMetadata", "character", function(project, path) {
    full.path <- file.path(project, path)

    # Obtaining the path to the actual metadata file. This may not exist for
    # metadata-only documents, which point to themselves (and should end be
    # JSON files and end with '.json')
    meta.path <- paste0(full.path, ".json")
    if (file.exists(meta.path)) {
        full.path <- meta.path
    } else if (!grepl("\\.json$", full.path)) {
        stop("'", path, "' has no accompanying JSON metadata file")
    }

    info <- fromJSON(full.path, simplifyVector=TRUE, simplifyMatrix=FALSE, simplifyDataFrame=FALSE)

    # If this is a redirect, follow it.
    if (dirname(info[["$schema"]]) == "redirection") {
        first <- info[["redirection"]][["targets"]][[1]]
        if (first$type == "local") {
            info <- acquireMetadata(project, first$location)
        } else {
            # TODO: allow applications to insert their own redirection handlers.
            stop("unknown '", first$type, "'-type redirect to '", info$location, "'")
        }
    }

    info
})
