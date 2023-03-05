#' Saving the metadata
#'
#' Helper function to write metadata from a named list to a JSON file.
#' This is commonly used inside \code{\link{stageObject}} methods to create the metadata file for a child object.
#'
#' @param meta A named list containing metadata.
#' This should contain at least the \code{"$schema"} and \code{"path"} elements.
#' @param dir String containing a path to the staging directory.
#' @param ignore.null Logical scalar indicating whether \code{NULL} values should be ignored during coercion to JSON.
#'
#' @details
#' Any \code{NULL} values in \code{meta} are pruned out prior to writing when \code{ignore.null=TRUE}.
#' This is done recursively so any \code{NULL} values in sub-lists of \code{meta} are also ignored.
#'
#' Any scalars are automatically unboxed so array values should be explicitly specified as such with \code{\link{I}()}.
#'
#' Any starting \code{"./"} in \code{meta$path} will be automatically removed.
#' This allows staging methods to save in the current directory by setting \code{path="."},
#' without the need to pollute the paths with a \code{"./"} prefix.
#'
#' The JSON-formatted metadata is validated against the schema in \code{meta[["$schema"]]} using \pkg{jsonvalidate}.
#' The location of the schema is taken from the \code{package} attribute in that string, if one exists;
#' otherwise, it is assumed to be in the \pkg{alabaster.schemas} package.
#' (All schemas are assumed to live in the \code{inst/schemas} subdirectory of their indicated packages.)
#'
#' We also use the schema to determine whether \code{meta} refers to an actual artifact or is a metadata-only document.
#' If it refers to an actual file, we compute its MD5 sum and store it in the metadata for saving.
#' We also save its associated metadata into a JSON file at a location obtained by appending \code{".json"} to \code{meta$path}.
#'
#' For artifacts, the MD5 sum calculation will be skipped if the \code{meta} already contains a \code{md5sum} field.
#' This can be useful on some occasions, e.g., to improve efficiency when the MD5 sum was already computed during staging,
#' or if the artifact does not actually exist in its full form on the file system.
#'
#' @return A JSON file containing the metadata is created at \code{path}.
#' A list of resource metadata is returned, e.g., for inclusion as the \code{"resource"} property in parent schemas.
#'
#' @author Aaron Lun
#' @examples
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#'
#' tmp <- tempfile()
#' dir.create(tmp)
#' info <- stageObject(df, tmp, path="coldata")
#' .writeMetadata(info, tmp)
#' cat(readLines(file.path(tmp, "coldata/simple.csv.gz.json")), sep="\n")
#'
#' @export
#' @rdname writeMetadata
#' @importFrom jsonlite toJSON fromJSON
.writeMetadata <- function(meta, dir, ignore.null=TRUE) { 
    if (ignore.null) {
        meta <- .strip_null_from_list(meta)
    }

    schema.id <- meta[["$schema"]]
    pkg <- attr(schema.id, "package")
    if (is.null(pkg)) {
        pkg <- "alabaster.schemas"
    }

    schema <- system.file("schemas", schema.id, package=pkg)
    if (schema == "") {
        stop("failed to find '", schema.id, "' in package '", pkg, "'")
    }

    # Avoid an extra look-up from file.
    if (schema %in% names(schema.details$is.meta)) {
        meta.only <- schema.details$is.meta[[schema]]
    } else {
        meta.only <- isTRUE(fromJSON(schema)[["_attributes"]][["metadata_only"]])
        schema.details$is.meta[[schema]] <- meta.only
    }

    meta$path <- gsub("^\\./", "", meta$path) # stripping this out for convenience.
    if (grepl("\\\\", meta$path)) {
        stop("Windows-style path separators are not allowed")
    }

    jpath <- meta$path
    if (!meta.only) {
        jpath <- paste0(jpath, ".json")
        if (is.null(meta$md5sum) && dirname(schema.id) != "redirection") {
            meta$md5sum <- digest::digest(file=file.path(dir, meta$path))
        }
    }

    jpath <- file.path(dir, jpath)
    write(file=jpath, toJSON(meta, pretty=TRUE, auto_unbox=TRUE, digits=NA))
    jsonvalidate::json_validate(jpath, schema, error=TRUE, engine="ajv") 

    list(type="local", path=meta$path)
}

.strip_null_from_list <- function(x) {
    keep <- !logical(length(x))
    for (i in seq_along(x)) {
        current <- x[[i]]
        if (is.list(current)) {
            x[[i]] <- .strip_null_from_list(current)
        } else if (is.null(current)) {
            keep[i] <- FALSE
        }
    }
    x[keep]
}

schema.details <- new.env()
schema.details$is.meta <- list()
