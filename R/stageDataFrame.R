#' Stage a DataFrame
#'
#' Stage a DataFrame by saving it to a CSV or HDF5 file.
#' CSV files follow the \href{https://github.com/LTLA/comservatory}{comservatory} specification,
#' while the expected layout of a HDF5 file is described in the \code{hdf5_data_frame} schema in \pkg{alabaster.schemas}.
#'
#' @param x A \linkS4class{DataFrame}.
#' @inheritParams stageObject
#' @param df.name String containing the relative path inside \code{dir} to save the CSV/HDF5 file.
#' @param mcols.name String specifying the name of the directory inside \code{path} to save \code{\link{mcols}(x)}.
#' If \code{NULL}, per-element metadata is not saved.
#' @param meta.name String specifying the name of the directory inside \code{path} to save \code{\link{metadata}(x)}.
#' If \code{NULL}, object metadata is not saved.
#' @param .version.df,.version.hdf5 Internal use only.
#'
#' @return
#' A named list containing the metadata for \code{x}.
#' \code{x} itself is written to a CSV or HDF5 file inside \code{path}.
#' Additional files may also be created inside \code{path} and referenced from the metadata.
#'
#' @details
#' All atomic vector types are supported in the columns along with dates and (ordered) factors.
#' Dates and factors are converted to character vectors and saved as such inside the file.
#' Factor levels are saved in a separate data frame, which is referenced in the \code{columns} field of the returned metadata.
#'
#' Any non-atomic columns are saved to a separate file inside \code{path} via \code{\link{stageObject}},
#' and referenced from the corresponding \code{columns} entry.
#' For consistency, they will be replaced in the main file by a placeholder all-zero column.
#'
#' As a DataFrame is a \linkS4class{Vector} subclass, its R-level metadata can be staged by \code{\link{.processMetadata}}.
#'
#' @section File formats:
#' If \code{\link{.saveDataFrameFormat}() == "csv"}, the contents of \code{x} are saved to a uncompressed CSV file.
#' If \code{x} has non-\code{NULL} row names, the first saved column in the CSV is named \code{row_names} and will contain the row names.
#' This should be ignored when indexing columns and comparing them to the corresponding entry of \code{columns} in the file's JSON metadata document.
#'
#' If \code{\link{.saveDataFrameFormat}() == "csv.gz"}, the CSV file is compressed (the default).
#' This reduces space and bandwidth requirements at the cost of the (de)compression overhead.
#' It also makes it more difficult to do queries inside the file without decompression of the entire file.
#'
#' If \code{\link{.saveDataFrameFormat}() == "hdf5"}, \code{x} is saved into a HDF5 file instead of a CSV.
#' Columns are saved into a \code{data} group where each column is a dataset named after its positional index.
#' The names of the columns are saved into the \code{column_names} dataset.
#' If row names are present, a separate \code{row_names} dataset containing the row names will be generated.
#' This format is most useful for random access and for preserving the precision of numerical data.
#'
#' @seealso
#' \url{https://github.com/LTLA/comservatory}, for the CSV file specification.
#'
#' The \code{csv_data_frame} and \code{hdf5_data_frame} schemas from the \pkg{alabaster.schemas} package.
#'
#' 
#' @author Aaron Lun
#'
#' @examples
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#'
#' tmp <- tempfile()
#' dir.create(tmp)
#' stageObject(df, tmp, path="coldata")
#'
#' list.files(tmp, recursive=TRUE)
#' 
#' @export 
#' @rdname stageDataFrame
#' @importFrom utils write.csv
#' @importFrom S4Vectors DataFrame
setMethod("stageObject", "DataFrame", function(x, dir, path, child=FALSE, df.name="simple", mcols.name="mcols", meta.name="other", .version.df=2, .version.hdf5=3) {
    dir.create(file.path(dir, path), showWarnings=FALSE)

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

        if (.version.hdf5 < 3) {
            sanitized <- .sanitize_df_columns(x, dir, path, .version.df)
            meta <- sanitized$metadata
            factor.levels <- sanitized$levels
            .dump_df_to_hdf5(sanitized$x, meta, "contents", ofile, .version.hdf5=.version.hdf5)
            extra[[1]]$version <- .version.hdf5
        } else {
            meta <- .write_hdf5_new(x, "contents", ofile)
        }

        schema <- "hdf5_data_frame/v1.json"
        extra[[1]]$group <- "contents"

        check_hdf5_df(ofile, 
            name=extra[[1]]$group,
            nrows=nrow(x), 
            has_row_names=has_row_names,
            column_names=true.colnames,
            column_types=vapply(meta, function(x) x$type, ""),
            string_formats=vapply(meta, function(x) if (is.null(x$format)) "" else x$format, ""),
            factor_levels=factor.levels,
            factor_ordered=vapply(meta, function(x) if (is.null(x$ordered)) FALSE else x$ordered, TRUE),
            df_version=.version.df,
            hdf5_version=.version.hdf5
        )

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
        check_csv_df(ofile, 
            nrows=nrow(x), 
            has_row_names=has_row_names,
            column_names=true.colnames,
            column_types=vapply(meta, function(x) x$type, ""),
            string_formats=vapply(meta, function(x) if (is.null(x$format)) "" else x$format, ""),
            factor_levels=sanitized$levels,
            factor_ordered=vapply(meta, function(x) if (is.null(x$ordered)) FALSE else x$ordered, TRUE),
            df_version=.version.df,
            is_compressed=extra[[1]]$compression == "gzip",
            parallel=TRUE
        )
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
            version=if (.version.df != 1) .version.df else NULL 
        )
    )

    names(extra) <- dirname(schema) 
    meta <- c(meta, extra)
    meta
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
            x[[z]] <- .sanitize_datetime(col)

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

#' @importFrom rhdf5 h5write h5createGroup h5createFile H5Gopen H5Gclose H5Acreate H5Aclose H5Awrite H5Fopen H5Fclose H5Dopen H5Dclose
.write_hdf5_new <- function(x, host, ofile) {
    h5createFile(ofile)
    prefix <- function(x) paste0(host, "/", x)
    h5createGroup(ofile, host)
    h5createGroup(ofile, paste0(host, "/data"))

    fhandle <- H5Fopen(ofile)
    on.exit(H5Fclose(fhandle), add=TRUE)
    (function (){
        ghandle <- H5Gopen(fhandle, host)
        on.exit(H5Gclose(ghandle), add=TRUE)
        h5writeAttribute("1.0", ghandle, "version", asScalar=TRUE)
        ahandle <- H5Acreate(ghandle, "row-count", "H5T_NATIVE_UINT32", H5Screate("H5S_SCALAR"))
        on.exit(H5Aclose(ahandle), add=TRUE)
        H5Awrite(ahandle, nrow(x))
    })()

    meta <- vector("list", ncol(x))
    for (z in seq_along(meta)) {
        col <- x[[z]]
        data.name <- as.character(z - 1L)
        colmeta <- list(name=true.colnames[z])
        is.other <- FALSE
        sanitized <- NULL

        if (length(dim(col)) > 1) {
            is.other <- TRUE

        } else if (is.factor(col)) {
            colmeta$type <- "factor"
            if (is.ordered(col)) {
                colmeta$ordered <- TRUE
            }

            full.data.name <- prefix(paste0("/data/", data.name))
            h5createGroup(fhandle, data.name)
            (function() {
                ghandle <- H5Gopen(fhandle, data.name)
                on.exit(H5Gclose(ghandle), add=TRUE)
                h5writeAttribute("factor", ghandle, "type", asScalar=TRUE)
                if (is.ordered(col)) {
                    h5writeAttribute(1, ghandle, "ordered", asScalar=TRUE)
                }
            })()

            code.name <- paste0(full.data.name, "/codes")
            h5write(as.integer(col), fhandle, code.name)
            h5write(levels(col), fhandle, paste0(full.data.name, "/levels"));
            if (anyNA(col)) {
                addMissingPlaceholderAttributeForHdf5(fhandle, code.name, -1L)
            }

        } else if (.is_datetime(col)) {
            colmeta$type <- "string"
            colmeta$format <- "date-time"
            sanitized <- .sanitize_datetime(col)

        } else if (is(col, "Date")) {
            colmeta$type <- "string"
            colmeta$format <- "date"
            sanitized <- .sanitize_date(col)

        } else if (is.atomic(col)) {
            coerced <- .remap_atomic_type(col)
            colmeta$type <- coerced$type
            sanitized <- coerced$values

        } else {
            is.other <- TRUE
        }

        if (is.other) {
            colmeta$type <- "other"
            tryCatch({
                other.info <- altStageObject(x[[z]], dir, paste0(path, "/column", z), child=TRUE)
                colmeta$resource <- writeMetadata(other.info, dir=dir)
            }, error = function(e) stop("failed to stage column '", colmeta$name, "'\n  - ", e$message))

        } else if (!is.null(sanitized)) {
            transformed <- transformVectorForHdf5(sanitized)
            current <- transformed$transformed
            missing.placeholder <- transformed$placeholder

            full.data.name <- prefix(paste0("data/", data.name))
            h5write(current, fhandle, full.data.name)
            if (!is.null(missing.placeholder)) {
                addMissingPlaceholderAttributeForHdf5(fhandle, full.data.name, missing.placeholder)
            }

            (function() {
                dhandle <- H5Dopen(fhandle, full.data.name)
                on.exit(H5Dclose(dhandle), add=TRUE)
                h5writeAttribute(colmeta$type, dhandle, "type", asScalar=TRUE)
                if (!is.null(colmeta$format)) {
                    h5writeAttribute(colmeta$format, dhandle, "format", asScalar=TRUE)
                }
            })()
        }

        meta[[z]] <- colmeta
    }

    h5write(colnames(x), ofile, prefix("column_names"))
    if (!is.null(rownames(x))) {
        h5write(rownames(x), ofile, prefix("row_names"))
    }

    meta
}
