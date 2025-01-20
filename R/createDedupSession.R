#' Deduplicate objects when saving
#'
#' Utilities for deduplicating objects inside \code{\link{saveObject}} to save time and/or storage space.
#'
#' @param x Some object, typically S4.
#' @param session Session object created by \code{\link{createDedupSession}}.
#' @param path String containing the absolute path to the directory in which \code{x} is to be saved.
#' This will be used for deduplication if another object is identified as a duplicate of \code{x}.
#' If a relative path is provided, it will be converted to an absolute path.
#'
#' @return 
#' \code{createDedupSession} will return a deduplication session that can be modified in-place.
#'
#' If \code{x} is a duplicate of an object in \code{session}, \code{checkObjectInDedupSession} will return a string containing the absolute path to a directory representing that object.
#' Otherwise, it will return \code{NULL}.
#'
#' \code{addObjectToDedupSession} will add \code{x} to \code{session} with the supplied \code{path}.
#' It returns \code{NULL} invisibly.
#'
#' @author Aaron Lun
#'
#' @details
#' These utilities allow extension developers to support deduplication of objects in a top-level call to \code{\link{saveObject}}.
#' For a given \code{saveObject} method, we can:
#' \enumerate{
#' \item Accept a session object in an optional \code{<PREFIX>.dedup.session=} argument.
#' We may also accept a \code{<PREFIX>.dedup.action=} argument to specify how any deduplication should be performed.
#' Some \code{<PREFIX>} prefix should be chosen to avoid conflicts between multiple deduplication sessions.
#' \item If a session argument is provided, we call \code{checkObjectInDedupSession(x, session)} to see if the \code{x} is a duplicate of an existing object in the session.
#' If a path is returned, we call \code{\link{cloneDirectory}} and return.
#' \item Otherwise, we save this object to disk, possibly passing the session argument as \code{<PREFIX>.dedup.session=} in further calls to \code{saveObject} for child objects.
#' We call \code{addObjectToDedupSession} to add the current object to the session.
#' }
#' A user can enable deduplication by passing the output of \code{createDedupSession} to \code{<PREFIX>.dedup.session=} in the top-level call to \code{saveObject}.
#' This is most typically performed when saving SummarizedExperiment objects with multiple assays, where one assay consists of delayed operations on another assay.
#' 
#' @examples
#' test <- function(x, path, test.dedup.session=NULL, test.dedup.action="link") {
#'    if (!is.null(test.dedup.session)) {
#'        original <- checkObjectInDedupSession(x, test.dedup.session)
#'        if (!is.null(original)) {
#'            cloneDirectory(original, path, test.dedup.action)
#'            return(invisible(NULL))
#'        }
#'    }
#'    dir.create(path)
#'    saveRDS(x, file.path(path, "whee.rds")) # replace this with actual saving code.
#'    if (!is.null(test.dedup.session)) {
#'        addObjectToDedupSession(x, test.dedup.session, path)
#'    }
#' }
#'
#' library(S4Vectors)
#' y <- DataFrame(A=1:10, B=1:10)
#' tmp <- tempfile()
#' dir.create(tmp)
#'
#' # Saving the first instance of the object, which is now stored in the session.
#' session <- createDedupSession()
#' checkObjectInDedupSession(y, session) # no duplicates yet.
#' test(y, file.path(tmp, "first"), test.dedup.session=session)
#' 
#' # Saving it again will trigger the deduplication.
#' checkObjectInDedupSession(y, session)
#' test(y, file.path(tmp, "duplicate"), test.dedup.session=session)
#'
#' list.files(tmp, recursive=TRUE)
#'
#' @export
createDedupSession <- function() {
    output <- new.env()
    output$known <- list()
    output
}

#' @export
#' @rdname createDedupSession
checkObjectInDedupSession <- function(x, session) {
    if (is.null(session)) {
        return(NULL)
    }

    cls <- as.character(class(x))[1]
    if (!(cls %in% names(session$known))) {
        return(NULL)
    }

    candidates <- session$known[[cls]]
    for (y in candidates) {
        if (identical(x, y$value)) {
            return(y$path)
        }
    }

    return(NULL)
}

#' @export
#' @rdname createDedupSession
addObjectToDedupSession <- function(x, session, path) {
    cls <- as.character(class(x))[1]
    if (!(cls %in% names(session$known))) {
        session$known[[cls]] <- list()
    }

    # Convert into an absolute path to protect against changes in the working directory.
    path <- absolutizePath(path) 

    session$known[[cls]] <- c(session$known[[cls]], list(list(value=x, path=path)))
    invisible(NULL)
}
