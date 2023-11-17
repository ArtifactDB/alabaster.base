#' Read R-level metadata
#'
#' Read \code{\link{metadata}} and \code{\link{mcols}} for a \linkS4class{Annotated} or \linkS4class{Vector} object, respectively.
#' This is typically used inside loading functions for concrete subclasses.
#'
#' @param x An \linkS4class{Vector} or \linkS4class{Annotated} object.
#' @param mcols.path String containing a path to a directory, itself containing an on-disk representation of a \linkS4class{DataFrame} to be used as the \code{\link{mcols}}.
#' @param metadata.path String containing a path to a directory, itself containing an on-disk representation of a base R list to be used as the \code{\link{metadata}}.
#' @param ... Further arguments to be passed to \code{\link{altReadObject}}.
#'
#' @author Aaron Lun
#'
#' @return \code{x} is returned, possibly with \code{mcols} and \code{metadata} added to it.
#'
#' @seealso
#' \code{\link{saveMetadata}}, which does the staging.
#'
#' @export
#' @aliases restoreMetadata .restoreMetadata
#' @importFrom S4Vectors mcols<- metadata<-
readMetadata <- function(x, metadata.path, mcols.path, ...) {
    if (file.exists(metadata.path)) {
        metadata(x) <- altReadObject(metadata.path, ...)
    }

    if (file.exists(mcols.path)) {
        mcols(x) <- altReadObject(mcols.path, ...)
    }

    x
}

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
restoreMetadata <- function(x, mcol.data, meta.data, project) { 
    if (!is.null(mcol.data)) {
        rd.info <- acquireMetadata(project, mcol.data$resource$path)
        mcols(x) <- altLoadObject(rd.info, project)
    }

    if (!is.null(meta.data)) {
        meta.info <- acquireMetadata(project, meta.data$resource$path)
        metadata(x) <- altLoadObject(meta.info, project)
    }

    x
}

# Soft-deprecated back-compatibility fixes

#' @export
.restoreMetadata <- function(...) restoreMetadata(...)
