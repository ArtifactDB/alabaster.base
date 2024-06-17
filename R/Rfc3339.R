#' Representing Internet date/times
#'
#' The Rfc3339 class is a character vector that stores Internet Date/time timestamps, formatted as described in RFC3339.
#' It provides a faithful representation of any RFC3339-compliant string in an R session.
#'
#' @param x For \code{as.Rfc3339} methods, object to be coerced to an Rfc3339 instance.
#'
#' For the subset and combining methods, an Rfc3339 instance.
#'
#' For \code{as.character}, \code{as.POSIXlt} and \code{as.POSIXct} methods, an Rfc3339 instance.
#'
#' For \code{is.Rfc3339}, any object to be tested for Rfc3339-ness.
#' @param i Indices specifying elements to extract or replace.
#' @param value Replacement values, either as another Rfc3339 instance, a character vector or something that can be coerced into one.
#' @param tz,recursive,... Further arguments to be passed to individual methods.
#' @param path String containing the path to a directory in which to save \code{x}.
#'
#' @return
#' For \code{as.Rfc3339}, the subset and combining methods, an Rfc3339 instance is returned.
#'
#' For the other \code{as.*} methods, an instance of the corresponding type generated from an Rfc3339 instance.
#'
#' @details
#' This class is motivated by the difficulty in using the various \link{POSIXt} classes to faithfully represent any RFC3339-compliant string.
#' In particular:
#' \itemize{
#' \item The POSIXt classes do not automatically capture the string's timezone offset, instead converting all times to the local timezone.
#' This is problematic as it discards information about the original timezone.
#' Technically, the \link{POSIXlt} class is capable of holding this information in the \code{gmtoff} field but it is not clear how to set this.
#' \item There is no way to distinguish between the timezones \code{Z} and \code{+00:00}.
#' These are functionally the same but will introduce differences in the checksums of saved files
#' and thus interfere with deduplication mechanisms in storage backends.
#' \item Coercion of POSIXt classes to strings may print more or fewer digits in the fractional seconds than what was present in the original string.
#' Functionally, this is probably unimportant but will still introduce differences in the checksums.
#' }
#'
#' By comparison, the Rfc3339 class preserves all information in the original string,
#' avoiding unexpected modifications from a roundtrip through \code{\link{readObject}} and \code{\link{saveObject}}.
#' This is especially relevant for strings that were created from other languages, 
#' e.g., Node.js Date's ISO string conversion uses \code{Z} by default.
#'
#' That said, users should not expect too much from this class.
#' It is only used to provide a faithful representation of RFC3339 strings, and does not support any time-related arithmetic.
#' Users are advised to convert to \link{POSIXct} or similar if such operations are required.
#' 
#' @author Aaron Lun
#' @examples
#' out <- as.Rfc3339(Sys.time() + 1:10)
#' out
#'
#' out[2:5]
#' out[2] <- "2"
#' c(out, out)
#'
#' as.character(out)
#' as.POSIXct(out)
#' @name Rfc3339
NULL

Rfc3339 <- function(x) {
    class(x) <- "Rfc3339"
    x
}

#' @export
#' @rdname Rfc3339
as.Rfc3339 <- function(x) UseMethod("as.Rfc3339")

#' @export
#' @rdname Rfc3339
as.Rfc3339.character <- function(x) {
    fail <- not_rfc3339(x)
    if (any(fail)) {
        x[fail] <- NA_character_
    }
    Rfc3339(x)
}

#' @export
#' @rdname Rfc3339
as.Rfc3339.default <- function(x) {
    if (is.Rfc3339(x)) {
        x
    } else {
        as.Rfc3339.character(as.character(x))
    }
}

#' @export
#' @rdname Rfc3339
as.Rfc3339.POSIXt <- function(x) as.Rfc3339.character(.sanitize_datetime(x))

#' @export
#' @rdname Rfc3339
as.character.Rfc3339 <- function(x, ...) {
    unclass(x)
}

#' @export
#' @rdname Rfc3339
is.Rfc3339 <- function(x) {
    inherits(x, "Rfc3339")
}

.cast_datetime <- function(x, output=as.POSIXct, ...) {
    zend <- endsWith(x, "Z")

    if (any(zend, na.rm=TRUE)) {
        # strptime doesn't know how to handle 'Z' offsets.
        ziend <- which(zend)
        xz <- x[ziend]
        x[ziend] <- sprintf("%s+0000", substr(xz, 1L, nchar(xz)-1L))
    }

    if (!all(zend, na.rm=TRUE)) {
        # Remove colon in the timezone, which confuses as.POSIXct().
        nzend <- which(!zend)
        x[nzend] <- sub(":([0-9]{2})$", "\\1", x[nzend])
    }

    # Remove fractional seconds.
    x <- sub("\\.[0-9]+", "", x)

    output(x, format="%Y-%m-%dT%H:%M:%S%z", ...)
}

#' @export
#' @rdname Rfc3339
as.POSIXct.Rfc3339 <- function(x, tz="", ...) .cast_datetime(unclass(x), tz=tz, output=as.POSIXct, ...)

#' @export
#' @rdname Rfc3339
as.POSIXlt.Rfc3339 <- function(x, tz="", ...) .cast_datetime(unclass(x), tz=tz, output=as.POSIXlt, ...)

#' @export
#' @rdname Rfc3339
`[.Rfc3339` <- function(x, i) Rfc3339(as.character(x)[i])

#' @export
#' @rdname Rfc3339
`[[.Rfc3339` <- function(x, i) Rfc3339(as.character(x)[[i]])

#' @export
#' @rdname Rfc3339
`[<-.Rfc3339` <- function(x, i, value) {
    y <- unclass(x)
    y[i] <- unclass(as.Rfc3339(value))
    Rfc3339(y)
}

#' @export
#' @rdname Rfc3339
`[[<-.Rfc3339` <- function(x, i, value) {
    y <- unclass(x)
    y[[i]] <- unclass(as.Rfc3339(value))
    Rfc3339(y)
}

#' @export
#' @rdname Rfc3339
c.Rfc3339 <- function(..., recursive=TRUE) {
    combined <- list(...)
    for (i in seq_along(combined)) {
        combined[[i]] <- unclass(as.Rfc3339(combined[[i]]))
    }
    combined <- do.call(c, combined)
    Rfc3339(combined)
}

#' @export
setOldClass("Rfc3339")

#' @export
#' @rdname Rfc3339
setMethod("saveObject", "Rfc3339", function(x, path, ...) .save_atomic_vector(x, path, ...)) 
# Put it here to ensure Rfc3339 is defined... we wrap the .save_atomic_vector call
# in a function so it doesn't get evaluated before saveAtomicVector.R is collated.
