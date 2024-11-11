#' Quickly read and write a CSV file
#'
#' Quickly read and write a CSV file, usually as a part of staging or loading a larger object.
#' This assumes that all files follow the \href{https://github.com/ArtifactDB/comservatory}{comservatory} specification.
#'
#' @param path String containing a path to a CSV to read/write.
#' @param expected.columns Named character vector specifying the type of each column in the CSV (excluding the first column containing row names, if \code{row.names=TRUE}).
#' @param row.names For \code{.quickReadCsv}, a logical scalar indicating whether the CSV contains row names. 
#'
#' For \code{.quickWriteCsv}, a logical scalar indicating whether to save the row names of \code{df}.
#' @param parallel Whether reading and parsing should be performed concurrently.
#' @param expected.nrows Integer scalar specifying the expected number of rows in the CSV.
#' @param compression String specifying the compression that was/will be used.
#' This should be either \code{"none"}, \code{"gzip"}.
#' @param df A \link[S4Vectors]{DFrame} or data.frame object, containing only atomic columns.
#' @param ... Further arguments to pass to \code{\link{write.csv}}.
#' @param validate Whether to double-check that the generated CSV complies with the comservatory specification.
#'
#' @author Aaron Lun
#' 
#' @return For \code{.quickReadCsv}, a \link[S4Vectors]{DFrame} containing the contents of \code{path}.
#'
#' For \code{.quickWriteCsv}, \code{df} is written to \code{path} and a \code{NULL} is invisibly returned.
#'
#' @examples
#' library(S4Vectors)
#' df <- DataFrame(A=1, B="Aaron")
#'
#' temp <- tempfile()
#' .quickWriteCsv(df, path=temp, row.names=FALSE, compression="gzip")
#'
#' .quickReadCsv(temp, c(A="numeric", B="character"), 1, "gzip", FALSE)
#' 
#' @export
#' @aliases .quickReadCsv .quickWriteCsv
#' @importFrom S4Vectors DataFrame
quickReadCsv <- function(path, expected.columns, expected.nrows, compression, row.names, parallel=TRUE) {
    df <- read.csv3(path, compression, expected.nrows, parallel)

    if (row.names) {
        if (ncol(df) < 1) {
            stop("expected at least one column in the CSV for row name retrieval")
        }
        rownames(df) <- df[,1]
        df <- df[,-1,drop=FALSE]
    }

    if (ncol(df) != length(expected.columns)) {
        stop("different number of columns in CSV than expected from metadata (", ncol(df), " vs ", length(expected.columns)) 
    }

    for (i in seq_along(expected.columns)) {
        expected.type <- expected.columns[[i]]
        if (!is(df[[i]], expected.type)) {
            before <- is.na(df[[i]])
            df[[i]] <- as(df[[i]], expected.type)
            after <- is.na(df[[i]])
            if (!identical(before, after)) {
                stop("coercion to ", expected.type, " introduced NAs for column ", i, " in the CSV")
            }
        }
    }

    df <- DataFrame(df, check.names=FALSE)
    colnames(df) <- names(expected.columns)

    df
}

read.csv3 <- function(path, compression, nrows, parallel=TRUE) {
    path <- normalizePath(path, mustWork=TRUE)
    df <- load_csv(path, is_compressed=identical(compression, "gzip"), nrecords = nrows, parallel=parallel)
    if (length(df)) {
        df <- data.frame(df, check.names=FALSE)
    } else {
        dummy <- matrix(0, attr(df, "num.records"), 0)
        df <- data.frame(dummy)
    }
    df
}

#' @export
#' @rdname quickReadCsv
#' @importFrom utils write.csv
quickWriteCsv <- function(df, path, ..., row.names=FALSE, compression="gzip", validate=TRUE) {
    .quick_write_csv(df=df, path=path, ..., row.names=row.names, compression=compression)
    if (validate) {
        check_csv(path, is_compressed=identical(compression, "gzip"), parallel=TRUE)
    }
}

.quick_write_csv <- function(df, path, ..., row.names=FALSE, compression="gzip") {
    if (compression == "gzip") {
        handle <- gzfile(path, "wb")
    } else {
        handle <- file(path, "wb")
    }
    on.exit(close(handle))

    if (ncol(df) == 0 && (!row.names || is.null(rownames(df)))) {
        # Avoid creating an empty header.
        writeLines(character(nrow(df) + 1L), sep="\n", con=handle)
    } else {
        to_quote <- !logical(ncol(df))

        # write.table saves NaNs as NA, and setting na= will convert all NA's,
        # so we have to convert to string here and then refuse quoting for those columns.
        for (i in seq_len(ncol(df))) {
            col <- df[[i]]
            if (is.double(col) && anyNA(col) && sum(is.nan(col)) > 0) {
                df[[i]] <- as.character(col)
                to_quote[i] <- FALSE
            }
        }

        if (all(to_quote)) {
            quote <- TRUE
        } else {
            quote <- which(to_quote)
        }
        write.csv(as.data.frame(df), file=handle, row.names=row.names, quote=quote, ..., eol="\n")
    }
}

# Soft-deprecated back-compatibility fixes.

#' @export
.quickReadCsv <- function(...) quickReadCsv(...)

#' @export
.quickWriteCsv <- function(...) quickWriteCsv(...)
