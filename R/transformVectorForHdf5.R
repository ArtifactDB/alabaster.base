#' Transform a vector to save in a HDF5 file
#'
#' This handles type casting and missing placeholder value selection/substitution.
#' It is primarily intended for developers of \pkg{alabaster.*} extensions.
#'
#' @param x An atomic vector to be saved to HDF5.
#' @param .version Internal use only.
#' 
#' @return
#' A list containing:
#' \itemize{
#' \item \code{transformed}, the transformed vector.
#' This may be the same as \code{x} if no \code{NA} values were detected.
#' Note that logical vectors are cast to integers.
#' \item \code{placeholder}, the placeholder value used to represent \code{NA} values.
#' This is \code{NULL} if no \code{NA} values were detected in \code{x},
#' otherwise it is the same as the output of \code{\link{chooseMissingPlaceholderForHdf5}}.
#' }
#'
#' @author Aaron Lun
#' @examples
#' transformVectorForHdf5(c(TRUE, NA, FALSE))
#' transformVectorForHdf5(c(1L, NA, 2L))
#' transformVectorForHdf5(c(1L, NaN, 2L))
#' transformVectorForHdf5(c("FOO", NA, "BAR"))
#' transformVectorForHdf5(c("FOO", NA, "NA"))
#'
#' @export
transformVectorForHdf5 <- function(x, .version=3) {
    placeholder <- NULL
    if (is.logical(x)) {
        storage.mode(x) <- "integer"
        if (anyNA(x)) {
            placeholder <- -1L
            x[is.na(x)] <- placeholder
        }

    } else if (is.character(x)) {
        x <- enc2utf8(x) # avoid mis-encoding multi-byte characters from Latin-1.
        if (anyNA(x)) {
            placeholder <- chooseMissingPlaceholderForHdf5(x)
            x[is.na(x)] <- placeholder
        }

    } else if (is.double(x)) {
        if (any_actually_numeric_na(x)) {
            placeholder <- chooseMissingPlaceholderForHdf5(x, .version=.version)
            if (!any_actually_numeric_na(placeholder)) {
                x[is_actually_numeric_na(x)] <- placeholder
            }
        }

    } else {
        if (anyNA(x)) {
            placeholder <- as(NA, storage.mode(x))
        }
    }

    list(transformed = x, placeholder = placeholder)
}
