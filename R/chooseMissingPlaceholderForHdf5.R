#' Choose a missing value placeholder
#'
#' In the \pkg{alabaster.*} framework, we mark missing entries inside HDF5 datasets with placeholder values.
#' This function chooses a value for the placeholder that does not overlap with anything else in a vector.
#'
#' @param x An atomic vector to be saved to HDF5.
#' @param .version Internal use only.
#' 
#' @return
#' A placeholder value for missing values in \code{x},
#' guaranteed to not be equal to any non-missing value in \code{x}.
#'
#' @details
#' For floating-point datasets, the placeholder will not be NA if there are mixtures of NAs and NaNs.
#' We do not rely on the NaN payload to distinguish between these two values.
#'
#' Placeholder values are typically saved as scalar attributes on the HDF5 dataset that they are used in.
#' The usual name of this attribute is \code{"missing-value-placeholder"}, as encoding by \code{missingPlaceholderName}.
#'
#' @examples
#' chooseMissingPlaceholderForHdf5(c(TRUE, NA, FALSE))
#' chooseMissingPlaceholderForHdf5(c(1L, NA, 2L))
#' chooseMissingPlaceholderForHdf5(c("aaron", NA, "barry"))
#' chooseMissingPlaceholderForHdf5(c("aaron", NA, "barry", "NA"))
#' chooseMissingPlaceholderForHdf5(c(1.5, NA, 2.6))
#' chooseMissingPlaceholderForHdf5(c(1.5, NaN, NA, 2.6))
#'
#' @aliases
#' missingPlaceholderName
#' addMissingPlaceholderAttributeForHdf5
#' .addMissingStringPlaceholderAttribute
#' .chooseMissingStringPlaceholder
#
#' @export
chooseMissingPlaceholderForHdf5 <- function(x, .version=3) {
    missing.placeholder <- NULL

    if (is.logical(x)) {
        missing.placeholder <- -1L

    } else if (is.character(x)) {
        missing.placeholder <- "NA"
        search <- unique(x)
        while (missing.placeholder %in% search) {
            missing.placeholder <- paste0("_", missing.placeholder)
        }

    } else if (is.double(x)) {
        if (.version < 3) {
            missing.placeholder <- NA_real_
        } else {
            missing.placeholder <- choose_numeric_missing_placeholder(x)
        }

    } else {
        missing.placeholder <- as(NA, storage.mode(x))
    }

    missing.placeholder
}

#' @export
missingPlaceholderName <- "missing-value-placeholder"

# Soft-deprecated back-compatibility fixes.

#' @export
addMissingPlaceholderAttributeForHdf5 <- function(file, name, placeholder) {
    if (is.character(file)) {
        file <- H5Fopen(file)
        on.exit(H5Fclose(file), add=TRUE)
    }
    dhandle <- H5Dopen(file, name)
    on.exit(H5Dclose(dhandle), add=TRUE)
    h5writeAttribute(placeholder, h5obj=dhandle, name=missingPlaceholderName, asScalar=TRUE)
}

#' @export
.chooseMissingStringPlaceholder <- function(...) chooseMissingPlaceholderForHdf5(...)

#' @export
.addMissingStringPlaceholderAttribute <- function(...) addMissingPlaceholderAttributeForHdf5(...)
