#' Stage a DataFrame
#'
#' Stage a DataFrame by saving it to a CSV or HDF5 file.
#'
#' @param x A \linkS4class{DataFrame}.
#' @inheritParams stageObject
#' @param df.name String containing the relative path inside \code{dir} to save the CSV/HDF5 file.
#' @param mcols.name String specifying the name of the directory inside \code{path} to save \code{\link{mcols}(x)}.
#' If \code{NULL}, per-element metadata is not saved.
#' @param meta.name String specifying the name of the directory inside \code{path} to save \code{\link{metadata}(x)}.
#' If \code{NULL}, object metadata is not saved.
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
#' @aliases
#' .addStringPlaceholderAttribute
#' .chooseStringPlaceholder
#' 
#' @rdname stageDataFrame
#' @importFrom utils write.csv
#' @importFrom S4Vectors DataFrame
setMethod("stageObject", "DataFrame", function(x, dir, path, child=FALSE, df.name="simple", mcols.name="mcols", meta.name="other") {
    dir.create(file.path(dir, path), showWarnings=FALSE)

    # Fix to ensure that DFs with duplicate names are properly saved;
    # otherwise any [[<- will call make.names.
    true.colnames <- colnames(x)
    colnames(x) <- sprintf("V%s", seq_len(ncol(x)))

    # Returning the metadata about the column type.
    meta <- vector("list", ncol(x))
    for (z in seq_along(meta)) {
        col <- x[[z]]
        out <- list(name=true.colnames[z])
        is.other <- FALSE

        if (length(dim(col)) < 2) { # only vectors or 1-d arrays.
            if (is.factor(col)) {
                if (is.ordered(col)) {
                    out$type <- "ordered"
                } else { 
                    out$type <- "factor"
                }

                if (anyNA(col) && any(levels(col) == "NA")) {
                    stop("cannot have both missing values and 'NA' levels")
                }

                tryCatch({
                     lev.info <- .stageObject(DataFrame(levels=levels(col)), dir, file.path(path, paste0("column", z)), df.name="levels", child=TRUE)
                     out$levels <- list(resource=.writeMetadata(lev.info, dir=dir))
                 }, error = function(e) stop("failed to stage levels of factor column '", out$name, "'\n  - ", e$message))

                x[[z]] <- as.character(col)

            } else if (is.character(col)) {
                out$type <- "string"

            } else if (is(col, "Date")) {
                out$type <- "date"
                x[[z]] <- format(col, "%Y-%m-%d")

            } else if (is.atomic(col)) {
                out$type <- .remap_type(col)

            } else {
                is.other <- TRUE
            }
        } else {
            is.other <- TRUE
        }

        if (is.other) {
            out$type <- "other"

            tryCatch({
                other.info <- .stageObject(x[[z]], dir, file.path(path, paste0("column", z)), child=TRUE)
                out$resource <- .writeMetadata(other.info, dir=dir)
            }, error = function(e) stop("failed to stage column '", out$name, "'\n  - ", e$message))

            x[[z]] <- integer(nrow(x))
        }

        meta[[z]] <- out
    }

    # Restoring the true colnames.
    colnames(x) <- true.colnames

    # Saving contents to file.
    format <- .saveDataFrameFormat()
    opath <- file.path(path, df.name)
    extra <- list(list())

    if (!is.null(format) && format=="hdf5") {
        opath <- paste0(opath, ".h5")
        ofile <- file.path(dir, opath)
        .write_hdf5_data_frame(x, ofile)
        schema <- "hdf5_data_frame/v1.json"
        extra[[1]]$group <- "data"

    } else {
        X <- data.frame(x, check.names=FALSE)
        if (!is.null(rownames(x))) {
            # Using the 'row_names' name for back-compatibility.
            X <- cbind(row_names=rownames(x), X)
        }
        
        if (!is.null(format) && format=="csv") {
            opath <- paste0(opath, ".csv")
            ofile <- file.path(dir, opath)
            .quickWriteCsv(X, ofile, row.names=FALSE, compression="none")
            extra[[1]]$compression <- "none"
        } else {
            opath <- paste0(opath, ".csv.gz")
            ofile <- file.path(dir, opath)
            .quickWriteCsv(X, ofile, row.names=FALSE, compression="gzip")
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
            row_names=!is.null(rownames(x)),
            column_data=element_data,
            other_data=other_data,
            dimensions=dim(x)
        )
    )

    names(extra) <- dirname(schema) 
    meta <- c(meta, extra)
    meta
})

.remap_type <- function(x) {
    y <- typeof(x)
    switch(y,
        integer="integer",
        double="number",
        numeric="number",
        logical="boolean",
        stop("type '", y, "' is not supported")
    )
}

#' @importFrom rhdf5 h5write h5createGroup h5createFile 
.write_hdf5_data_frame <- function(x, ofile) {
    h5createFile(ofile)
    h5createGroup(ofile, "data")

    for (i in seq_along(x)) {
        current <- x[[i]]
        if (is.logical(current)) {
            # The logical'ness of this column is preserved in the metadata,
            # so we can always convert it back later.
            current <- as.integer(current) 
        }

        missing.placeholder <- NULL
        if (is.character(current) && anyNA(current)) {
            missing.placeholder <- .chooseMissingStringPlaceholder(current)
            current[is.na(current)] <- missing.placeholder
        }

        data.name <- as.character(i - 1L)
        h5write(current, ofile, file.path("data", data.name))

        if (!is.null(missing.placeholder)) {
            .addMissingStringPlaceholderAttribute(ofile, paste0("data/", data.name), missing.placeholder)
        }
    }

    h5write(colnames(x), ofile, "column_names")
    if (!is.null(rownames(x))) {
        h5write(rownames(x), ofile, "row_names")
    }
}

# Exported for re-use in anything that saves possibly-missing strings to HDF5.
# alabaster.matrix is probably the prime suspect here.

#' @export
.chooseMissingStringPlaceholder <- function(x) {
    missing.placeholder <- "NA"
    search <- unique(x)
    while (missing.placeholder %in% search) {
        missing.placeholder <- paste0("_", missing.placeholder)
    }
    missing.placeholder
}

#' @export
#' @importFrom rhdf5 H5Fopen H5Fclose H5Dopen H5Dclose h5writeAttribute
.addMissingStringPlaceholderAttribute <- function(file, path, placeholder) {
    fhandle <- H5Fopen(file)
    on.exit(H5Fclose(fhandle), add=TRUE)
    dhandle <- H5Dopen(fhandle, path)
    on.exit(H5Dclose(dhandle), add=TRUE)
    h5writeAttribute(placeholder, h5obj=dhandle, name="missing-value-placeholder", asScalar=TRUE)
}
