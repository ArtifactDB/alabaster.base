#' Save a DataFrame to disk
#'
#' Stage a DataFrame by saving it to a HDF5 file.
#'
#' @param x A \linkS4class{DataFrame}.
#' @inheritParams saveObject
#'
#' @return
#' A named list containing the metadata for \code{x}.
#' \code{x} itself is written to a CSV or HDF5 file inside \code{path}.
#' Additional files may also be created inside \code{path} and referenced from the metadata.
#'
#' @details
#' This method creates a \code{basic_columns.h5} file that contains columns for atomic vectors, factors, dates and date-times.
#' Dates and date-times are converted to character vectors and saved as such inside the file.
#' Factors are saved as a HDF5 group with both the codes and the levels as separate datasets.
#'
#' Any non-atomic columns are saved to a \code{other_columns} subdirectory inside \code{path} via \code{\link{saveObject}},
#' named after its zero-based positional index within \code{x}.
#'
#' If \code{\link{metadata}} or \code{\link{mcols}} are present, 
#' they are saved to the \code{other_annotations} and \code{column_annotations} subdirectories, respectively, via \code{\link{saveObject}}.
#'
#' @author Aaron Lun
#'
#' @examples
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#'
#' tmp <- tempfile()
#' saveObject(df, tmp)
#' list.files(tmp, recursive=TRUE)
#' 
#' @export 
#' @rdname stageDataFrame
#' @aliases stageObject,DataFrame-method
#' @importFrom S4Vectors DataFrame
setMethod("saveObject", "DataFrame", function(x, path, ...) {
    dir.create(path, showWarnings=FALSE)
    .write_hdf5_new(x, path, ...)
    saveMetadata(
        x,
        metadata.path=file.path(path, "other_annotations"),
        mcols.path=file.path(path, "column_annotations"),
        ...
    )
    saveObjectFile(path, "data_frame", list(data_frame=list(version="1.0")))
})

#' @importFrom rhdf5 h5write h5createGroup h5createFile H5Gopen H5Gclose H5Acreate H5Aclose H5Awrite H5Fopen H5Fclose H5Dopen H5Dclose
.write_hdf5_new <- function(x, path, ...) {
    subpath <- "basic_columns.h5"
    ofile <- paste0(path, "/", subpath)

    fhandle <- H5Fcreate(ofile, "H5F_ACC_TRUNC")
    on.exit(H5Fclose(fhandle), add=TRUE, after=FALSE)
    ghandle <- H5Gcreate(fhandle, "data_frame")
    on.exit(H5Gclose(ghandle), add=TRUE, after=FALSE)
    h5_write_attribute(ghandle, "row-count", nrow(x), scalar=TRUE, type="H5T_NATIVE_UINT32")

    gdhandle <- H5Gcreate(ghandle, "data")
    on.exit(H5Gclose(gdhandle), add=TRUE, after=FALSE)

    collected <- list()
    for (z in seq_len(ncol(x))) {
        col <- x[[z]]
        data.name <- as.character(z - 1L)
        is.other <- FALSE
        sanitized <- NULL
        coltype <- NULL
        colformat <- NULL

        if (is.factor(col)) {
            local({
                colhandle <- H5Gcreate(gdhandle, data.name)
                on.exit(H5Gclose(colhandle), add=TRUE, after=FALSE)
                h5_write_attribute(colhandle, "type", "factor", scalar=TRUE)
                if (is.ordered(col)) {
                    h5_write_attribute(colhandle, "ordered", 1L, scalar=TRUE)
                }
                .simple_save_codes(colhandle, col, save.names=FALSE)
                h5_write_vector(colhandle, "levels", levels(col))
            })

        } else if (.is_datetime(col)) {
            coltype <- "string"
            colformat <- "date-time"
            sanitized <- as.character(as.Rfc3339(col))

        } else if (is(col, "Date")) {
            coltype <- "string"
            colformat <- "date"
            sanitized <- .sanitize_date(col)

        } else if (is.atomic(col)) {
            if (length(dim(col)) > 1) {
                is.other <- TRUE
            } else {
                coerced <- .remap_atomic_type(col)
                coltype <- coerced$type
                sanitized <- coerced$values
            }

        } else {
            is.other <- TRUE
        }

        if (is.other) {
            other.dir <- file.path(path, "other_columns")
            dir.create(other.dir, showWarnings=FALSE)
            tryCatch({
                altSaveObject(x[[z]], file.path(other.dir, data.name), ...)
            }, error = function(e) stop("failed to stage column '", colnames(x)[z], "'\n  - ", e$message))

        } else if (!is.null(sanitized)) {
            transformed <- transformVectorForHdf5(sanitized)
            current <- transformed$transformed
            missing.placeholder <- transformed$placeholder

            local({
                dhandle <- h5_write_vector(gdhandle, data.name, current, emit=TRUE)
                on.exit(H5Dclose(dhandle), add=TRUE, after=FALSE)
                if (!is.null(missing.placeholder)) {
                    h5_write_attribute(dhandle, missingPlaceholderName, missing.placeholder, scalar=TRUE)
                }

                h5_write_attribute(dhandle, "type", coltype, scalar=TRUE)
                if (!is.null(colformat)) {
                    h5_write_attribute(dhandle, "format", colformat, scalar=TRUE)
                }
            })
        }
    }

    h5_write_vector(ghandle, "column_names", colnames(x))
    if (!is.null(rownames(x))) {
        h5_write_vector(ghandle, "row_names", rownames(x))
    }
}

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
#' @importFrom utils write.csv
setMethod("stageObject", "DataFrame", function(x, dir, path, child=FALSE, df.name="simple", mcols.name="mcols", meta.name="other", .version.df=2, .version.hdf5=2) {
    full.path <- file.path(dir, path)
    dir.create(full.path, showWarnings=FALSE)

    true.colnames <- colnames(x)
    if (anyDuplicated(true.colnames)) {
        stop("detected duplicate column names in a ", class(x)[1], " object")
    }
    if (any(true.colnames == "")) {
        stop("detected empty column name in a ", class(x)[1], " object")
    }

    # Saving contents to file.
    format <- .saveDataFrameFormat()
    opath <- paste0(path, "/", df.name)
    extra <- list(list())
    has_row_names = !is.null(rownames(x))

    if (!is.null(format) && format=="hdf5") {
        opath <- paste0(opath, ".h5")
        ofile <- file.path(dir, opath)
        factor.levels <- rep(list(character(0)), ncol(x))

        sanitized <- .sanitize_df_columns(x, dir, path, .version.df)
        meta <- sanitized$metadata
        factor.levels <- sanitized$levels
        .dump_df_to_hdf5(sanitized$x, meta, "contents", ofile, .version.hdf5=.version.hdf5)
        extra[[1]]$version <- min(.version.hdf5, 3)

        schema <- "hdf5_data_frame/v1.json"
        extra[[1]]$group <- "contents"

    } else {
        sanitized <- .sanitize_df_columns(x, dir, path, .version.df)
        meta <- sanitized$metadata
        X <- data.frame(sanitized$x, check.names=FALSE)
        if (has_row_names) {
            # Using the 'row_names' name for back-compatibility.
            X <- cbind(row_names=rownames(sanitized$x), X)
        }
        
        if (!is.null(format) && format=="csv") {
            opath <- paste0(opath, ".csv")
            ofile <- file.path(dir, opath)
            .quickWriteCsv(X, ofile, row.names=FALSE, compression="none", validate=FALSE)
            extra[[1]]$compression <- "none"
        } else {
            opath <- paste0(opath, ".csv.gz")
            ofile <- file.path(dir, opath)
            .quickWriteCsv(X, ofile, row.names=FALSE, compression="gzip", validate=FALSE)
            extra[[1]]$compression <- "gzip"
        }

        schema <- "csv_data_frame/v1.json"
    }

    element_data <- .processMcols(x, dir, path, mcols.name)
    other_data <- .processMetadata(x, dir, path, meta.name)

    meta <- list(
        `$schema`=schema,
        path=opath,
        is_child=child,
        data_frame=list(
            columns=meta,
            row_names=has_row_names,
            column_data=element_data,
            other_data=other_data,
            dimensions=dim(x),
            version=if (.version.df != 1) min(.version.df, 2) else NULL 
        )
    )

    names(extra) <- dirname(schema) 
    c(meta, extra)
})

.sanitize_df_columns <- function(x, dir, path, .version.df) {
    # Fix to ensure that DFs with invalid names are properly saved;
    # otherwise any [[<- will call make.names.
    true.colnames <- colnames(x)
    colnames(x) <- sprintf("V%s", seq_len(ncol(x)))

    meta <- vector("list", ncol(x))
    all.levels <- rep(list(character(0)), ncol(x))

    for (z in seq_along(meta)) {
        col <- x[[z]]
        out <- list(name=true.colnames[z])
        is.other <- FALSE

        if (length(dim(col)) > 1) {
            is.other <- TRUE
        } else if (is.factor(col)) {
            if (.version.df == 1) {
                if (is.ordered(col)) {
                    out$type <- "ordered"
                } else { 
                    out$type <- "factor"
                }

                tryCatch({
                     lev.info <- altStageObject(DataFrame(levels=levels(col)), dir, paste0(path, "/column", z), df.name="levels", child=TRUE)
                     out$levels <- list(resource=writeMetadata(lev.info, dir=dir))
                 }, error = function(e) stop("failed to stage levels of factor column '", out$name, "'\n  - ", e$message))

                x[[z]] <- as.character(col)

            } else {
                out$type <- "factor"
                if (is.ordered(col)) {
                    out$ordered <- TRUE
                }

                tryCatch({
                     lev.info <- altStageObject(levels(col), dir, paste0(path, "/column", z), df.name="levels", child=TRUE)
                     out$levels <- list(resource=writeMetadata(lev.info, dir=dir))
                 }, error = function(e) stop("failed to stage levels of factor column '", out$name, "'\n  - ", e$message))

                x[[z]] <- as.integer(col) - 1L
            }

            all.levels[[z]] <- levels(col)

        } else if (.is_datetime(col)) {
            if (.version.df == 1) {
                out$type <- "date-time"
            } else {
                out$type <- "string"
                out$format <- "date-time"
            }
            x[[z]] <- as.character(as.Rfc3339(col))

        } else if (is(col, "Date")) {
            if (.version.df == 1) {
                out$type <- "date"
            } else {
                out$type <- "string"
                out$format <- "date"
            }
            x[[z]] <- .sanitize_date(col)

        } else if (is.atomic(col)) {
            coerced <- .remap_atomic_type(col)
            out$type <- coerced$type
            x[[z]] <- coerced$values

        } else {
            is.other <- TRUE
        }

        if (is.other) {
            out$type <- "other"

            tryCatch({
                other.info <- altStageObject(x[[z]], dir, paste0(path, "/column", z), child=TRUE)
                out$resource <- writeMetadata(other.info, dir=dir)
            }, error = function(e) stop("failed to stage column '", out$name, "'\n  - ", e$message))

            x[[z]] <- integer(nrow(x))
        }

        meta[[z]] <- out
    }

    # Restoring the true colnames.
    colnames(x) <- true.colnames

    list(x=x, metadata=meta, levels=all.levels)
}

#' @importFrom rhdf5 h5write h5createGroup h5createFile
.dump_df_to_hdf5 <- function(x, column.meta, host, ofile, .version.hdf5) {
    h5createFile(ofile)
    prefix <- function(x) paste0(host, "/", x)
    h5createGroup(ofile, host)
    h5createGroup(ofile, prefix("data"))

    for (i in seq_along(x)) {
        curmeta <- column.meta[[i]]
        if (curmeta$type == "other") {
            next
        }
        current <- x[[i]]

        missing.placeholder <- NULL
        if (.version.hdf5 > 1) {
            transformed <- transformVectorForHdf5(current, .version=.version.hdf5)
            current <- transformed$transformed
            missing.placeholder <- transformed$placeholder
        } else {
            if (is.character(current)) {
                if (anyNA(current)) {
                    missing.placeholder <- chooseMissingPlaceholderForHdf5(current)
                    current[is.na(current)] <- missing.placeholder
                }
            } else if (is.logical(current)) {
                # The logical'ness of this column is preserved in the metadata,
                # so we can always convert it back later.
                current <- as.integer(current) 
            } 
        }

        data.name <- as.character(i - 1L)
        full.data.name <- prefix(paste0("data/", data.name))
        h5write(current, ofile, full.data.name)
        if (!is.null(missing.placeholder)) {
            addMissingPlaceholderAttributeForHdf5(ofile, full.data.name, missing.placeholder)
        }
    }

    h5write(colnames(x), ofile, prefix("column_names"))
    if (!is.null(rownames(x))) {
        h5write(rownames(x), ofile, prefix("row_names"))
    }
}
