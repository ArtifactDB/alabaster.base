#' Read a DataFrame from disk
#'
#' Read a \linkS4class{DataFrame} from its on-disk representation.
#' This is usually not directly called by users, but is instead called by dispatch in \code{\link{readObject}}
#'
#' @param path String containing a path to the directory, itself created with \code{\link{saveObject}} method for \linkS4class{DataFrame}s.
#' @param metadata Named list containing metadata for the object, see \code{\link{readObjectFile}} for details.
#' @param ... Further arguments, passed to \code{\link{altLoadObject}} for complex nested columns.
#'
#' @return The \linkS4class{DataFrame} represented by \code{path}.
#'
#' @seealso
#' \code{"\link{saveObject,DataFrame-method}"}, for the staging method.
#'
#' @author Aaron Lun
#'
#' @examples
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#'
#' tmp <- tempfile()
#' saveObject(df, tmp)
#' readObject(tmp)
#'
#' @export
#' @aliases loadDataFrame
#' @importFrom S4Vectors DataFrame make_zero_col_DFrame
readDataFrame <- function(path, metadata, ...) {
    fpath <- file.path(path, "basic_columns.h5")
    fhandle <- H5Fopen(fpath)
    on.exit(H5Fclose(fhandle), add=TRUE, after=FALSE)

    host <- "data_frame"
    ghandle <- H5Gopen(fhandle, host)
    on.exit(H5Gclose(ghandle), add=TRUE, after=FALSE)

    nrows <- h5_read_attribute(ghandle, "row-count")
    colnames <- h5_read_vector(ghandle, "column_names")
    rownames <- NULL
    if (h5_object_exists(ghandle, "row_names")) {
        rownames <- h5_read_vector(ghandle, "row_names")
    }

    gdhandle <- H5Gopen(ghandle, "data")
    on.exit(H5Gclose(gdhandle), add=TRUE, after=FALSE)
    all.children <- h5ls(gdhandle, recursive=FALSE, datasetinfo=FALSE)$name

    columns <- vector("list", length(colnames))
    for (col in seq_along(colnames)) {
        expected <- as.character(col - 1L)

        if (expected %in% all.children) {
            type <- local({
                precolhandle <- H5Oopen(gdhandle, expected)
                on.exit(H5Oclose(precolhandle), add=TRUE, after=FALSE)
                h5_read_attribute(precolhandle, "type")
            })

            if (type == "factor") {
                columns[[col]] <- local({
                    colhandle <- H5Gopen(gdhandle, expected)
                    on.exit(H5Gclose(colhandle), add=TRUE, after=FALSE)
                    codes <- .simple_read_codes(colhandle)
                    levels <- h5_read_vector(colhandle, "levels")
                    ordered <- h5_read_attribute(colhandle, "ordered", check=TRUE, default=NULL)
                    factor(levels[codes], levels=levels, ordered=isTRUE(ordered > 0L))
                })

            } else {
                columns[[col]] <- local({
                    colhandle <- H5Dopen(gdhandle, expected)
                    on.exit(H5Dclose(colhandle), add=TRUE, after=FALSE)
                    contents <- H5Dread(colhandle)

                    missing.placeholder <- h5_read_attribute(colhandle, missingPlaceholderName, check=TRUE, default=NULL)
                    contents <- h5_cast(contents, expected.type=type, missing.placeholder=missing.placeholder)

                    if (type == "string") {
                        if (H5Aexists(colhandle, "format")) {
                            format <- h5_read_attribute(colhandle, "format")
                            if (format == "date") {
                                contents <- as.Date(contents)
                            } else if (format == "date-time") {
                                contents <- as.Rfc3339(contents)
                            }
                        }
                    }

                    contents
                })
            }

        } else {
            columns[[col]] <- S4Vectors::I(altReadObject(file.path(path, "other_columns", expected), ...))
        }
    }
   
    names(columns) <- colnames
    if (length(columns) || !is.null(rownames)) {
        output <- DataFrame(columns, check.names=FALSE, row.names=rownames)
    } else {
        output <- make_zero_col_DFrame(nrow=nrows)
    }

    readMetadata(
        output,
        metadata.path=file.path(path, "other_annotations"),
        mcols.path=file.path(path, "column_annotations"),
        ...
    )
}

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
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

                if (!is.null(current)) {
                    attrs <- h5readAttributes(path, prefix(paste0("data/", d)))
                    current <- h5_cast(current, expected.type=NULL, missing.placeholder=attrs[[missingPlaceholderName]])
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
