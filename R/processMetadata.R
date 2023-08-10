#' Process R-level metadata
#'
#' Stage \code{\link{metadata}} and \code{\link{mcols}} for a \linkS4class{Annotated} or \linkS4class{Vector} object, respectively.
#' This is typically used inside \code{\link{stageObject}} methods for concrete subclasses.
#'
#' @param x An \linkS4class{Vector} object for \code{.processMcols}, and a \linkS4class{Annotated} object for \code{.processMetadata}.
#' @param dir String containing the path to a staging directory.
#' @param path String containing the path within \code{dir} that is used to save all of \code{x}.
#' This should be the same as that passed to the \code{\link{stageObject}} method.
#' @param mcols.name String containing the name of the file to save \code{\link{mcols}(x)}.
#' If \code{NULL}, no saving is performed.
#' @param meta.name String containing the name of the file to save \code{\link{metadata}(x)}.
#' If \code{NULL}, no saving is performed.
#'
#' @author Aaron Lun
#'
#' @return 
#' Both functions return a list containing \code{resource}, itself a list specifying the path within \code{dir} where the metadata was saved.
#' Alternatively \code{NULL} if no saving is performed.
#'
#' @details
#' If \code{mcols(x)} has no columns, nothing is saved by \code{.processMcols}.
#' Similarly, if \code{metadata(x)} is an empty list, nothing is saved by \code{.processMetadata}.
#' 
#' @seealso
#' \code{.restoreMetadata}, which does the loading.
#'
#' @export
#' @aliases .processMetadata .processMcols
#' @importFrom S4Vectors mcols
processMetadata <- function(x, dir, path, meta.name) {
    if (!is.null(meta.name) && length(metadata(x))) {
        tryCatch({
            meta <- altStageObject(metadata(x), dir, paste0(path, "/", meta.name), child=TRUE)
            list(resource=writeMetadata(meta, dir=dir))
        }, error=function(e) stop("failed to stage 'metadata(<", class(x)[1], ">)'\n  - ", e$message))
    } else { 
        NULL
    }
}

#' @export
#' @rdname processMetadata
#' @importFrom S4Vectors mcols metadata
processMcols <- function(x, dir, path, mcols.name) {
    if (!is.null(mcols.name) && !is.null(mcols(x)) && ncol(mcols(x))) {
        tryCatch({
            meta <- altStageObject(mcols(x), dir, paste0(path, "/", mcols.name), child=TRUE)
            list(resource=writeMetadata(meta, dir=dir))
        }, error=function(e) stop("failed to stage 'mcols(<", class(x)[1], ">)'\n  - ", e$message))
    } else { 
        NULL
    }
}

# Soft-deprecated back-compatibility fixes

#' @export
.processMetadata <- function(...) processMetadata(...)

#' @export
.processMcols <- function(...) processMcols(...)
