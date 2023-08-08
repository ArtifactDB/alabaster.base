#' Load all non-child objects in a directory
#'
#' As the title suggests, this function loads all non-child objects in a staging directory.
#' Children are used to assemble their parent objects and are not reported here.
#'
#' @param dir String containing a path to a staging directory.
#' @param ... Further arguments to pass to \code{\link{fetchObject}}.
#' @param redirect.action String specifying how redirects should be handled:
#' \itemize{
#' \item \code{"to"} will report an object at the redirection destination, not the redirection source.
#' \item \code{"from"} will report an object at the redirection source(s), not the destination.
#' \item \code{"both"} will report an object at both the redirection source(s) and destination.
#' }
#'
#' @return 
#' A named list is returned containing all (non-child) R objects in \code{dir}.
#'
#' @author Aaron Lun
#' @examples
#' tmp <- tempfile()
#' dir.create(tmp)
#'
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#' meta <- stageObject(df, tmp, path="whee")
#' .writeMetadata(meta, tmp)
#'
#' ll <- list(A=1, B=LETTERS, C=DataFrame(X=1:5))
#' meta <- stageObject(ll, tmp, path="stuff")
#' .writeMetadata(meta, tmp)
#'
#' redirect <- .createRedirection(tmp, "whoop", "whee/simple.csv.gz")
#' .writeMetadata(redirect, tmp)
#' 
#' all.meta <- loadAllObjects(tmp)
#' str(all.meta) 
#'
#' @aliases
#' .resolveRedirectedObjects
#'
#' @export
loadAllObjects <- function(dir, redirect.action = c("to", "from", "both")) {
    all.meta <- loadAllMetadata(dir, ignore.children=TRUE)

    collected <- list()
    redirects <- list()

    for (m in all.meta) {
        if (startsWith(m[["$schema"]], "redirection/")) {
            redirects[[m$path]] <- m$redirection$targets[[1]]$location
            next
        } else if (isTRUE(m$is_child)) {
            next
        }
        collected[[m$path]] <- .loadObject(m, dir)
    }

    redirect.action <- match.arg(redirect.action)
    if (redirect.action == "to") {
        return(collected)
    }

    resolved <- .resolveRedirectedObjects(collected, redirects) 
    if (redirect.action == "from") {
        return(c(collected[-m], resolved))
    } else {
        return(c(collected, resolved))
    }
}

#' @export
.resolveRedirectedObjects<- function(values, redirects) {
    host <- names(redirects)
    targets <- unlist(redirects)

    m <- match(targets, names(values))
    keep <- !is.na(m)
    m <- m[keep]
    host <- host[keep]

    copies <- current[m]
    names(copies) <- host
    copies
}
