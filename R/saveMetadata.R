#' Save R-level metadata to disk
#'
#' Save \code{\link{metadata}} and \code{\link{mcols}} for \linkS4class{Annotated} or \linkS4class{Vector} objects, respectively, to disk.
#' These are typically used inside \code{\link{saveObject}} methods for concrete subclasses.
#'
#' @param x A \linkS4class{Vector} or \linkS4class{Annotated} object.
#' @param metadata.path String containing the path in which to save the \code{metadata}.
#' If \code{NULL}, no \code{\link{metadata}} is saved.
#' @param mcols.path String containing the path in which to save the \code{mcols}.
#' If \code{NULL}, no \code{\link{mcols}} is saved.
#' @param ... Further arguments to be passed to \code{\link{altStageObject}}.
#'
#' @author Aaron Lun
#'
#' @return 
#' The metadata for \code{x} is saved to \code{path}.
#'
#' @details
#' If \code{mcols(x)} has no columns, nothing is saved by \code{saveMcols}.
#' Similarly, if \code{metadata(x)} is an empty list, nothing is saved by \code{saveMetadata}.
#' This avoids creating unnecessary files with no meaningful content.
#'
#' If \code{mcols(x)} has non-\code{NULL} row names, these are removed prior to staging.
#' These names are usually redundant with the names associated with elements of \code{x} itself.
#'
#' @seealso
#' \code{\link{readMetadata}}, which restores metadata to the object.
#' 
#' @export
#' @aliases .processMetadata .processMcols processMetadata processMcols
#' @importFrom S4Vectors metadata
saveMetadata <- function(x, metadata.path, mcols.path, ...) {
    if (!is.null(metadata.path)) {
        mm <- metadata(x)
        if (!is.null(mm) && length(mm)) {
            tryCatch({
                altStageObject(mm, metadata.path, ...)
            }, error=function(e) stop("failed to stage 'metadata(<", class(x)[1], ">)'\n  - ", e$message))
        }
    }

    if (!is.null(mcols.path)) {
        mc <- mcols(x, use.names=FALSE)
        if (!is.null(mc) && ncol(mc)) {
            rownames(mc) <- NULL # stripping out unnecessary row names.
            output <- tryCatch({
                altStageObject(mc, mcols.path, ...)
            }, error=function(e) stop("failed to stage 'mcols(<", class(x)[1], ">)'\n  - ", e$message))
        }
    }
}

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
#' @importFrom S4Vectors metadata
processMetadata <- function(x, dir, path, meta.name) {
    if (!is.null(meta.name) && length(metadata(x))) {
        tryCatch({
            meta <- altStageObject(metadata(x), dir, paste0(path, "/", meta.name), child=TRUE, simplified=FALSE)
            list(resource=writeMetadata(meta, dir=dir))
        }, error=function(e) stop("failed to stage 'metadata(<", class(x)[1], ">)'\n  - ", e$message))
    } else { 
        NULL
    }
}

#' @export
#' @importFrom S4Vectors mcols
processMcols <- function(x, dir, path, mcols.name) {
    output <- NULL

    if (!is.null(mcols.name)) {
        mc <- mcols(x, use.names=FALSE)
        if (!is.null(mc) && ncol(mc)) {
            rownames(mc) <- NULL # stripping out unnecessary row names.
            output <- tryCatch({
                meta <- altStageObject(mc, dir, paste0(path, "/", mcols.name), child=TRUE, simplified=FALSE)
                list(resource=writeMetadata(meta, dir=dir))
            }, error=function(e) stop("failed to stage 'mcols(<", class(x)[1], ">)'\n  - ", e$message))
        }
    }

    return(output)
}

# Soft-deprecated back-compatibility fixes

#' @export
.processMetadata <- function(...) processMetadata(...)

#' @export
.processMcols <- function(...) processMcols(...)
