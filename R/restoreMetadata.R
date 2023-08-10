#' Restore R-level metadata
#'
#' Restore \code{\link{metadata}} and \code{\link{mcols}} for a \linkS4class{Annotated} or \linkS4class{Vector} object, respectively.
#' This is typically used inside loading functions for concrete subclasses.
#'
#' @param x An \linkS4class{Vector} or \linkS4class{Annotated} object.
#' @param mcol.data List of metadata specifying the \code{resource} location of the \code{\link{mcols}} files.
#' This can also be \code{NULL}, in which case no \code{mcols} are restored.
#' @param meta.data List of metadata specifying the \code{resource} location of the \code{\link{metadata}} files.
#' This can also be \code{NULL}, in which case no \code{metadata} are restored.
#' @inheritParams loadDataFrame
#'
#' @author Aaron Lun
#'
#' @return \code{x} is returned, possibly with \code{mcols} and \code{metadata} added to it.
#'
#' @seealso
#' \code{\link{.processMetadata}}, which does the staging.
#'
#' @export
#' @aliases .restoreMetadata
#' @importFrom S4Vectors mcols<- metadata<-
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
