#' Utilities for saving vectors to HDF5
#'
#' Exported for re-use in anything that saves possibly-missing values to HDF5.
#' \pkg{alabaster.matrix} is probably the main beneficiary here.
#'
#' @param x An atomic vector to be saved to HDF5.
#' @param file String containing the path to a HDF5 file.
#' @param name String containing the name of a HDF5 dataset.
#' @param placeholder Scalar value representing a placeholder for missing values.
#' 
#' @return
#' \code{chooseMissingPlaceholderForHdf5} returns a placeholder value for missing values in \code{x},
#' For strings, this is gauranteed to be an actual string that does not equal any existing entry.
#' For logicals, this is set to -1.
#' 
#' \code{transformVectorForHdf5} will transform an atomic vector in preparation for saving to HDF5, returning a list containing:
#' \itemize{
#' \item \code{transformed}, the transformed vector.
#' This may be the same as \code{x} if no \code{NA} values were detected.
#' Note that logical vectors are cast to integers.
#' \item \code{placeholder}, the placeholder value used to represent \code{NA} values.
#' This is \code{NULL} if no \code{NA} values were detected in \code{x},
#' otherwise it is the same as the output of \code{chooseMissingPlaceholderForHdf5}.
#' }
#'
#' \code{addMissingPlaceholderAttributeForHdf5} will add a \code{"missing-value-placeholder"} attribute to the HDF5 dataset,
#' containing the placeholder scalar used to represent missing values.
#'
#' @author Aaron Lun
#' @examples
#' transformVectorForHdf5(c(TRUE, NA, FALSE))
#' transformVectorForHdf5(c(1L, NA, 2L))
#' transformVectorForHdf5(c(1L, NaN, 2L))
#' transformVectorForHdf5(c("FOO", NA, "BAR"))
#' transformVectorForHdf5(c("FOO", NA, "NA"))
#'
#' @aliases
#' .addMissingStringPlaceholderAttribute
#' .chooseMissingStringPlaceholder
#'
#' @export
transformVectorForHdf5 <- function(x) {
    placeholder <- NULL
    if (is.logical(x)) {
        x <- as.integer(x)
        if (anyNA(x)) {
            placeholder <- -1L
            x[is.na(x)] <- placeholder
        }

    } else if (is.character(x)) {
        if (anyNA(x)) {
            placeholder <- chooseMissingPlaceholderForHdf5(x)
            x[is.na(x)] <- placeholder
        }

    } else if (is.double(x)) {
        if (anyNA(x) && sum(is.nan(x)) < sum(is.na(x))) {
            placeholder <- NA_real_
        }

    } else {
        if (anyNA(x)) {
            placeholder <- as(NA, storage.mode(x))
        }
    }

    list(transformed = x, placeholder = placeholder)
}

#' @export
#' @rdname transformVectorForHdf5
chooseMissingPlaceholderForHdf5 <- function(x) {
    missing.placeholder <- NULL

    if (is.logical(x)) {
        missing.placeholder <- -1L

    } else if (is.character(x)) {
        missing.placeholder <- "NA"
        search <- unique(x)
        while (missing.placeholder %in% search) {
            missing.placeholder <- paste0("_", missing.placeholder)
        }

    } else {
        missing.placeholder <- as(NA, storage.mode(x))
    }

    missing.placeholder
}

#' @export
#' @rdname transformVectorForHdf5
#' @importFrom rhdf5 H5Fopen H5Fclose H5Dopen H5Dclose h5writeAttribute
addMissingPlaceholderAttributeForHdf5 <- function(file, name, placeholder) {
    fhandle <- H5Fopen(file)
    on.exit(H5Fclose(fhandle), add=TRUE)
    dhandle <- H5Dopen(fhandle, name)
    on.exit(H5Dclose(dhandle), add=TRUE)
    h5writeAttribute(placeholder, h5obj=dhandle, name="missing-value-placeholder", asScalar=TRUE)
}

# Soft-deprecated back-compatibility fixes.

#' @export
.chooseMissingStringPlaceholder <- function(...) chooseMissingPlaceholderForHdf5(...)

#' @export
.addMissingStringPlaceholderAttribute <- function(...) addMissingPlaceholderAttributeForHdf5(...)