#' HDF5 storage type optimization
#'
#' Optimize HDF5 storage to use the smallest possible storage type. 
#' Intended for \pkg{alabaster.*} developers only.
#'
#' @param x An atomic vector or array of the specified type, 
#' e.g., integer vector/array for \code{collect_integer_attributes} and \code{optimize_integer_storage}.
#' Developers can also extend this to abstract objects containing values of the same type, e.g., matrix-like S4 classes.
#' @param fallback Function that accepts \code{x} and returns a suitable placeholder in the presence of \code{NA}s.
#' If \code{NULL}, this defaults to \code{\link{chooseMissingPlaceholderForHdf5}}.
#'
#' @return
#' For \code{collect_integer_attributes}, a named list containing:
#' \itemize{
#' \item \code{min}, the smallest non-\code{NA} value in \code{x}.
#' This is set to Inf if all values are \code{NA}.
#' \item \code{max}, the largest non-\code{NA} value in \code{x}.
#' This is set to -Inf if all values are \code{NA}.
#' \item \code{missing}, logical scalar indicating whether any values in \code{x} are \code{NA}.
#' }
#'
#' For \code{collect_number_attributes}, a named list containing:
#' \itemize{
#' \item \code{missing}, logical scalar indicating whether any values in \code{x} are \code{NA}.
#' \item \code{non_integer}, logical scalar indicating whether any values in \code{x} are non-integer.
#' \item \code{min}, the smallest non-\code{NA} integer value in \code{x}.
#' This is set to Inf if there are any non-integer or \code{NA} values.
#' \item \code{max}, the largest non-\code{NA} integer value in \code{x}.
#' This is set to -Inf if there are any non-integer or \code{NA} values.
#' \item \code{has_NaN}, logical scalar indicating whether NaN is present in \code{x}.
#' \item \code{has_Inf}, logical scalar indicating whether positive infinity is present in \code{x}.
#' \item \code{has_NegInf}, logical scalar indicating whether negative infinity is present in \code{x}.
#' \item \code{has_lowest}, logical scalar indicating whether the smallest double-precision value is present in \code{x}.
#' \item \code{has_highest}, logical scalar indicating whether the highest double-precision value is present in \code{x}.
#' }
#'
#' For \code{collect_string_attributes}, a named list containing:
#' \itemize{
#' \item \code{missing}, logical scalar indicating whether any values in \code{x} are \code{NA}.
#' \item \code{has_NA}, logical scalar indicating whether the \code{"NA"} string is present in \code{x}.
#' \item \code{has_NA_}, logical scalar indicating whether the \code{"NA_"} string is present in \code{x}.
#' \item \code{max_len}, integer scalar specifying the maximum length of the strings in \code{x}.
#' \item \code{has_non_utf8}, logical scalar indicating that \code{x} contains non-UTF8-encoded strings (e.g., Latin-1).
#' }
#'
#' For \code{collect_boolean_attributes}, a named list containing:
#' \itemize{
#' \item \code{missing}, logical scalar indicating whether any values in \code{x} are \code{NA}.
#' }
#'
#' For the \code{optimize_*_storage} functions, a named list containing:
#' \itemize{
#' \item \code{type}, string containing the HDF5 datatype for storing \code{x}.
#' \item \code{placeholder}, value of the placeholder for \code{NA} values.
#' \item \code{other}, other attributes of \code{x} (e.g., number of non-zero elements for sparse vectors).
#' These should be stored in an \code{other} field in the named list returned by \code{collect_*_ attributes}.
#' }
#'
#' @author Aaron Lun
#' @aliases
#' collect_integer_attributes,integer-method
#' collect_integer_attributes,array-method
#' collect_number_attributes,double-method
#' collect_number_attributes,array-method
#' collect_string_attributes,character-method
#' collect_string_attributes,array-method
#' collect_boolean_attributes,logical-method
#' collect_boolean_attributes,array-method
#' @name optimize_storage
NULL

#' @export
#' @rdname optimize_storage
setGeneric("collect_integer_attributes", function(x) standardGeneric("collect_integer_attributes"))

.collect_integer_attributes_raw <- function(x) {
    range <- suppressWarnings(range(x, na.rm=TRUE))
    list(
        min=range[1],
        max=range[2],
        missing=anyNA(x)
    )
}

#' @export
setMethod("collect_integer_attributes", "integer", .collect_integer_attributes_raw)

#' @export
setMethod("collect_integer_attributes", "array", .collect_integer_attributes_raw)

#' @export
#' @rdname optimize_storage
optimize_integer_storage <- function(x) {
    attr <- collect_integer_attributes(x)
    lower <- attr$min
    upper <- attr$max

    if (attr$missing) {
        # If it's infinite, that means that there are only missing values in
        # 'x', otherwise there should have been at least one finite value
        # available. In any case, it means we can just do whatever we want so
        # we'll just use the smallest type.
        if (is.infinite(lower)) {
            return(list(type="H5T_NATIVE_INT8", placeholder=as.integer(-2^7), other=attr$other))
        }

        if (lower < 0L) {
            if (lower > -2^7 && upper < 2^7) {
                return(list(type="H5T_NATIVE_INT8", placeholder=as.integer(-2^7), other=attr$other))
            } else if (lower > -2^15 && upper < 2^15) {
                return(list(type="H5T_NATIVE_INT16", placeholder=as.integer(-2^15), other=attr$other))
            }
        } else {
            if (upper < 2^8 - 1) {
                return(list(type="H5T_NATIVE_UINT8", placeholder=as.integer(2^8-1), other=attr$other))
            } else if (upper < 2^16 - 1) {
                return(list(type="H5T_NATIVE_UINT16", placeholder=as.integer(2^16-1), other=attr$other))
            }
        }

        return(list(type="H5T_NATIVE_INT32", placeholder=NA_integer_, other=attr$other))

    } else {
        # If it's infinite, that means that 'x' is of length zero, otherwise
        # there should have been at least one finite value available. Here,
        # the type doesn't matter, so we'll just use the smallest. 
        if (is.infinite(lower)) {
            return(list(type="H5T_NATIVE_INT8", placeholder=NULL, other=attr$other))
        }

        if (lower < 0L) {
            if (lower >= -2^7 && upper < 2^7) {
                return(list(type="H5T_NATIVE_INT8", placeholder=NULL, other=attr$other))
            } else if (lower >= -2^15 && upper < 2^15) {
                return(list(type="H5T_NATIVE_INT16", placeholder=NULL, other=attr$other))
            }
        } else {
            if (upper < 2^8) {
                return(list(type="H5T_NATIVE_UINT8", placeholder=NULL, other=attr$other))
            } else if (upper < 2^16) {
                return(list(type="H5T_NATIVE_UINT16", placeholder=NULL, other=attr$other))
            }
        }

        return(list(type="H5T_NATIVE_INT32", placeholder=NULL, other=attr$other))
    }
}

#' @export
#' @rdname optimize_storage
setGeneric("collect_number_attributes", function(x) standardGeneric("collect_number_attributes"))

#' @export
setMethod("collect_number_attributes", "double", collect_numeric_attributes)

#' @export
setMethod("collect_number_attributes", "array", collect_numeric_attributes)

#' @export
#' @rdname optimize_storage
optimize_number_storage <- function(x, fallback = chooseMissingPlaceholderForHdf5) {
    attr <- collect_number_attributes(x)
    lower <- attr$min
    upper <- attr$max

    if (attr$missing) {
        if (!attr$non_integer) {
            if (lower < 0L) {
                if (lower > -2^7 && upper < 2^7) {
                    return(list(type="H5T_NATIVE_INT8", placeholder=-2^7, other=attr$other))
                } else if (lower > -2^15 && upper < 2^15) {
                    return(list(type="H5T_NATIVE_INT16", placeholder=-2^15, other=attr$other))
                } else if (lower > -2^31 && upper < 2^31) {
                    return(list(type="H5T_NATIVE_INT32", placeholder=-2^31, other=attr$other))
                }
            } else {
                if (upper < 2^8-1) {
                    return(list(type="H5T_NATIVE_UINT8", placeholder=2^8-1, other=attr$other))
                } else if (upper < 2^16-1) {
                    return(list(type="H5T_NATIVE_UINT16", placeholder=2^16-1, other=attr$other))
                } else if (upper < 2^32-1) {
                    return(list(type="H5T_NATIVE_UINT32", placeholder=2^32-1, other=attr$other))
                }
            }
        }

        placeholder <- NULL
        if (!attr$has_NaN) {
            placeholder <- NaN
        } else if (!attr$has_Inf) {
            placeholder <- Inf
        } else if (!attr$has_NegInf) {
            placeholder <- -Inf
        } else if (!attr$has_lowest) {
            placeholder <- lowest_double()
        } else if (!attr$has_highest) {
            placeholder <- highest_double()
        }

        if (is.null(placeholder)) {
            if (is.null(fallback)) {
                fallback <- chooseMissingPlaceholderForHdf5
            }
            placeholder <- fallback(x)
        }

        return(list(type="H5T_NATIVE_DOUBLE", placeholder=placeholder, other=attr$other))

    } else {
        if (!attr$non_integer) {
            if (lower < 0L) {
                if (lower >= -2^7 && upper < 2^7) {
                    return(list(type="H5T_NATIVE_INT8", placeholder=NULL, other=attr$other))
                } else if (lower >= -2^15 && upper < 2^15) {
                    return(list(type="H5T_NATIVE_INT16", placeholder=NULL, other=attr$other))
                } else if (lower >= -2^31 && upper < 2^31) {
                    return(list(type="H5T_NATIVE_INT32", placeholder=NULL, other=attr$other))
                }
            } else {
                if (upper < 2^8) {
                    return(list(type="H5T_NATIVE_UINT8", placeholder=NULL, other=attr$other))
                } else if (upper < 2^16) {
                    return(list(type="H5T_NATIVE_UINT16", placeholder=NULL, other=attr$other))
                } else if (upper < 2^32) {
                    return(list(type="H5T_NATIVE_UINT32", placeholder=NULL, other=attr$other))
                }
            }
        }

        return(list(type="H5T_NATIVE_DOUBLE", placeholder=NULL, other=attr$other))
    }
}

#' @export
#' @rdname optimize_storage
setGeneric("collect_string_attributes", function(x) standardGeneric("collect_string_attributes"))

#' @export
setMethod("collect_string_attributes", "character", collect_character_attributes)

#' @export
setMethod("collect_string_attributes", "array", collect_character_attributes)

#' @export
#' @rdname optimize_storage
optimize_string_storage <- function(x, fallback = NULL) {
    attr <- collect_string_attributes(x)

    placeholder <- NULL
    if (attr$missing) {
        if (!attr[["has_NA"]]) {
            placeholder <- "NA"
        } else if (!attr[["has_NA_"]]) {
            placeholder <- "_NA"
        } else {
            if (is.null(fallback)) {
                fallback <- chooseMissingPlaceholderForHdf5
            }
            placeholder <- fallback(x)
        }
        attr$max_len <- max(attr$max_len, nchar(placeholder, "bytes"))
    }

    tid <- H5Tcopy("H5T_C_S1")
    H5Tset_strpad(tid, strpad = "NULLPAD")
    H5Tset_size(tid, max(1L, attr$max_len))
    H5Tset_cset(tid, "UTF8")

    list(type=tid, placeholder=placeholder, other=attr$other)
}

#' @export
#' @rdname optimize_storage
setGeneric("collect_boolean_attributes", function(x) standardGeneric("collect_boolean_attributes"))

.collect_boolean_attributes_raw <- function(x) list(list(missing=anyNA(x)))

#' @export
setMethod("collect_boolean_attributes", "logical", .collect_boolean_attributes_raw)

#' @export
setMethod("collect_boolean_attributes", "array", .collect_boolean_attributes_raw)

#' @export
#' @rdname optimize_storage
optimize_boolean_storage <- function(x) {
    attr <- collect_boolean_attributes(x)
    if (attr$missing) {
        list(type="H5T_NATIVE_INT8", placeholder=-1L, other=attr$other)
    } else {
        list(type="H5T_NATIVE_INT8", placeholder=NULL, other=attr$other)
    }
}
