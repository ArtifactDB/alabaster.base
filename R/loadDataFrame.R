#' Load a DataFrame 
#'
#' Load a \linkS4class{DataFrame} from file, possibly containing complex columns and row names.
#'
#' @param info Named list containing the metadata for this object.
#' @param project Any argument accepted by the acquisition functions, see \code{?\link{acquireFile}}.
#' By default, this should be a string containing the path to a staging directory.
#' @param include.nested Logical scalar indicating whether nested \linkS4class{DataFrame}s should be loaded.
#' @param parallel Whether to perform reading and parsing in parallel for greater speed.
#'
#' @details
#' This function effectively reverses the behavior of \code{"\link{stageObject,DataFrame-method}"}, 
#' loading the \linkS4class{DataFrame} back into memory from the CSV or HDF5 file.
#' Atomic columns are loaded directly while complex columns (such as nested DataFrames) are loaded by calling the appropriate \code{restore} method.
#' 
#' One implicit interpretation of using a nested DataFrame is that the contents are not important enough to warrant top-level columns.
#' In such cases, we can skip all columns containing a nested DataFrame by setting \code{include.nested=FALSE}.
#' This avoids the cost of loading a (potentially large) nested DataFrame when its contents are unlikely to be relevant.
#' 
#' @return The DataFrame described by \code{info}.
#'
#' @seealso
#' \code{"\link{stageObject,DataFrame-method}"}, for the staging method.
#'
#' @author Aaron Lun
#'
#' @examples
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#'
#' # First staging it:
#' tmp <- tempfile()
#' dir.create(tmp)
#' out <- stageObject(df, tmp, path="coldata")
#'
#' # And now loading it:
#' loadDataFrame(out, tmp)
#'
#' @export
#' @importFrom S4Vectors DataFrame make_zero_col_DFrame
#' @importFrom rhdf5 h5read h5readAttributes
loadDataFrame <- function(info, project, include.nested=TRUE, parallel=TRUE) {
    has.rownames <- isTRUE(info$data_frame$row_names)
    col.info <- info$data_frame$columns
    has.columns <- length(col.info) > 0
    nrows <- as.integer(info$data_frame$dimensions[[1]])
    if (!has.rownames && !has.columns) {
        return(make_zero_col_DFrame(nrow=nrows))
    }

    # Reading the file into a data frame.
    path <- acquireFile(project, info$path)
    if ("hdf5_data_frame" %in% names(info)) {
        prefix <- function(x) paste0(info$hdf5_data_frame$group, "/", x)
        if (!has.columns) {
            df <- make_zero_col_DFrame(nrow=nrows)
        } else {
            raw <- h5read(path, prefix("data"))
            df <- vector("list", length(col.info))

            for (i in seq_along(col.info)) {
                curinfo <- col.info[[i]]
                d <- as.character(i - 1L) # -1 to get back to 0-based indices.
                current <- raw[[d]]

                if (is.list(current)) { # Handling factors stored as lists in the new version.
                    if (curinfo$type != "factor") {
                        stop("HDF5 groups as columns are only supported for factor columns")
                    }
                    codes <- .repopulate_missing_hdf5(current$codes, path, prefix(paste0("data/", d, "/codes")))
                    df[[i]] <- factor(current$levels[codes + 1L], current$levels, ordered=isTRUE(curinfo$ordered))

                } else if (!is.null(current)) {
                    current <- .repopulate_missing_hdf5(current, path, prefix(paste0("data/", d)))
                    df[[i]] <- as.vector(current) # remove 1d arrays.

                } else {
                    df[[i]] <- logical(nrows) # placeholders
                }
            }

            names(df) <- as.vector(h5read(path, prefix("column_names")))
            df <- DataFrame(df, check.names=FALSE)
        }
        if (has.rownames) {
            rownames(df) <- as.vector(h5read(path, prefix("row_names")))
        }
    } else {
        df <- read.csv3(path, compression=info$csv_data_frame$compression, nrows=nrows)
        df <- DataFrame(df)
        if (has.rownames) {
            rownames(df) <- df[,1]
            df <- df[,-1,drop=FALSE]
        } else {
            rownames(df) <- NULL
        } 
    }

    df <- .coerce_df_column_type(df, col.info, project, include.nested=include.nested)
    .restoreMetadata(df, mcol.data=info$data_frame$column_data, meta.data=info$data_frame$other_data, project=project)
}

#' @importFrom rhdf5 h5readAttributes
.repopulate_missing_hdf5 <- function(current, path, name) {
    attr <- h5readAttributes(path, name)
    replace.na <- attr[["missing-value-placeholder"]]

    restore_min_integer <- function(y) {
        if (is.integer(y) && anyNA(y)) { # promote integer NAs back to the actual number.
            y <- as.double(y)
            y[is.na(y)] <- -2^31
        }
        y
    }

    if (is.null(replace.na)) {
        current <- restore_min_integer(current)
    } else if (is.na(replace.na)) {
        if (!is.nan(replace.na)) {
            # No-op as the placeholder is already R's NA of the relevant type.
        } else { 
            current[is.nan(current)] <- NA # avoid equality checks to an NaN.
        }
    } else {
        current <- restore_min_integer(current)
        current[which(current == replace.na)] <- NA # Using which() to avoid problems with existing NAs.
    }

    current
}

.coerce_df_column_type <- function(df, col.info, project, include.nested) {
    stopifnot(length(df) == length(col.info))
    true.names <- character(length(col.info))

    for (i in seq_along(col.info)) {
        current.info <- col.info[[i]]
        true.names[i] <- current.info$name
        col.type <- current.info$type 
        col <- df[[i]]

        if (col.type=="factor" || col.type=="ordered") {
            if (!is.factor(col)) { # we may have already transformed the column to a factor, in which case we can skip this.
                level.info <- acquireMetadata(project, current.info$levels$resource$path)
                levels <- altLoadObject(level.info, project=project)
                if (is(levels, "DataFrame")) { # account for old objects that store levels as a DF.
                    levels <- levels[,1]
                }
                if (is.numeric(col)) {
                    col <- levels[col + 1L]
                }
                ordered <- col.type == "ordered" || isTRUE(current.info$ordered)
                col <- factor(col, levels=levels, ordered=ordered)
            }

        } else if (col.type=="date") {
            col <- as.Date(col)

        } else if (col.type=="date-time") {
            col <- .cast_datetime(col)

        } else if (.is_atomic(col.type)) {
            if (col.type == "integer") {
                if (is.double(col) && any(col == -2^31, na.rm=TRUE)) {
                    # Don't cast to an integer if there's the special -2^31 value.
                } else {
                    col <- .cast_atomic(col, col.type)
                }
            } else if (col.type == "string") {
                f <- current.info$format
                if (identical(f, "date")) {
                    col <- as.Date(col)
                } else if (identical(f, "date-time")) {
                    col <- .cast_datetime(col)
                }
            } else {
                col <- .cast_atomic(col, col.type)
            }

        } else if (col.type == "other") {
            current <- acquireMetadata(project, current.info$resource$path)
            if (include.nested || !("data_frame" %in% names(current))) {
                col <- altLoadObject(current, project=project)
            } else {
                true.names[i] <- NA_character_
            }
        } else {
            stop("unsupported column type '", col.type, "'")
        }

        df[[i]] <- col
    }

    # Removing nested DFs.
    if (!all(keep <- !is.na(true.names))) {
        df <- df[,keep,drop=FALSE]
        true.names <- true.names[keep]
    }

    colnames(df) <- true.names

    df
}
