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

            # Replacing NAs for strings.
            for (i in names(raw)) {
                current <- raw[[i]]
                if (is.character(current)) {
                    attr <- h5readAttributes(path, prefix(paste0("data/", i)))
                    replace.na <- attr[["missing-value-placeholder"]]
                    if (!is.null(replace.na)) {
                        raw[[i]][current == replace.na] <- NA
                    }
                }
            }

            # Adding placeholders for type:"other".
            indices <- as.integer(names(raw)) + 1L # get back to 1-based.
            df <- vector("list", length(col.info))
            df[indices] <- lapply(raw, as.vector) # remove 1d arrays.
            for (i in seq_along(df)) {
                if (is.null(df[[i]])) {
                    df[[i]] <- logical(nrows) 
                }
            }

            df <- DataFrame(df)
            colnames(df) <- as.vector(h5read(path, prefix("column_names")))
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

    # Make sure everyone is of the right type.
    new.names <- character(ncol(df))

    for (i in seq_along(col.info)) {
        current.info <- col.info[[i]]
        new.names[i] <- current.info$name

        col.type <- current.info$type 
        col <- df[[i]]

        if (col.type=="factor" || col.type=="ordered") {
            level.info <- acquireMetadata(project, current.info$levels$resource$path)
            levels <- altLoadObject(level.info, project=project)
            if (is(levels, "DataFrame")) { # account for old objects that store levels as a DF.
                levels <- levels[,1]
            }
            if (is.numeric(col)) {
                col <- levels[col]
            }
            ordered <- col.type == "ordered" || isTRUE(current.info$ordered)
            col <- factor(col, levels=levels, ordered=ordered)

        } else if (col.type=="date") {
            col <- as.Date(col)

        } else if (col.type=="date-time") {
            col <- .cast_datetime(col)

        } else if (.is_atomic(col.type)) {
            col <- .cast_atomic(col, col.type)
            if (col.type == "string") {
                f <- current.info$format
                if (identical(f, "date")) {
                    col <- as.Date(col)
                } else if (identical(f, "date-time")) {
                    col <- .cast_datetime(col)
                }
            }

        } else if (col.type == "other") {
            current <- acquireMetadata(project, current.info$resource$path)
            if (include.nested || !("data_frame" %in% names(current))) {
                col <- altLoadObject(current, project=project)
            } else {
                new.names[i] <- NA_character_
            }
        } else {
            stop("unsupported column type '", col.type, "'")
        }

        df[[i]] <- col
    }

    # Removing nested DFs.
    if (!all(keep <- !is.na(new.names))) {
        df <- df[,keep,drop=FALSE]
        new.names <- new.names[keep]
    }

    # Replacing the names with the values at input.
    colnames(df) <- new.names

    .restoreMetadata(df, mcol.data=info$data_frame$column_data, meta.data=info$data_frame$other_data, project=project)
}
