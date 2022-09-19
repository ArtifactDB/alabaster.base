#' Getting unique values
#'
#' Get unique values, usually for the names or row names.
#' This is helpful for storing some values in the metadata for use in searching without creating excessively large documents.
#'
#' @param x A character vector or \code{NULL}.
#'
#' @return 
#' The first 100 unique values from \code{x}, or \code{NULL} if \code{x=NULL}.
#'
#' @author Aaron Lun
#'
#' @export
#' @rdname uniqueValues
#' @importFrom utils head
.uniqueValues <- function(x) {
    if (is.null(x)) {
        NULL
    } else {
        u <- unique(x)
        u <- u[!is.na(u)]
        I(as.character(head(u, 100)))
    }
}
