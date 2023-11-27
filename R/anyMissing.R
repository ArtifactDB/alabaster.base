#' Find missing values
#'
#' Find missing (\code{NA}) values.
#' This is smart enough to distinguish them from NaN values in numeric \code{x}.
#' For all other types, it just calls \code{\link{is.na}} or \code{\link{anyNA}}.
#'
#' @param x Vector or array of atomic values.
#'
#' @return 
#' For \code{anyMissing}, a logical scalar indicating whether any \code{NA} values were present in \code{x}.
#'
#' For \code{is.missing}, a logical vector or array of shape equal to \code{x}, indicating whether each value is \code{NA}.
#'
#' @author Aaron Lun
#'
#' @examples
#' anyNA(c(NaN))
#' anyNA(c(NA))
#' anyMissing(c(NaN))
#' anyMissing(c(NA))
#'
#' is.na(c(NA, NaN))
#' is.missing(c(NA, NaN))
#' @export
anyMissing <- function(x) {
    if (is.double(x)) {
        any_actually_numeric_na(x)
    } else {
        anyNA(x)
    }
}

#' @export
#' @rdname anyMissing
is.missing <- function(x) {
    if (is.double(x)) {
        out <- is_actually_numeric_na(x)
        if (!is.null(dim(x))) {
            dim(out) <- dim(x)
        }
        out
    } else {
        is.na(x)
    }
}
