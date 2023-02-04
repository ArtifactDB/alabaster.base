#' Stage a DataFrameFactor object
#'
#' Stage a \linkS4class{DataFrameFactor} object, a generalization of the base factor for \linkS4class{DataFrame} levels.
#'
#' @param x A \linkS4class{DataFrameFactor} object.
#' @inheritParams stageObject
#' @param index.name String containing the name of the file to save the factor indices.
#' @param level.name String containing the name of the subdirectory to save the factor levels.
#' @param mcols.name String specifying the name of the directory inside \code{path} to save the \code{\link{mcols}}.
#' If \code{NULL}, the metadata columns are not saved.
#'
#' @details
#' We create one file in \code{path} for the levels and another file for the factor indices. 
#' The levels file contains a DataFrame as staged by \code{\link{stageObject,DataFrame-method}}.
#' Indices are 1-based and reference one record of the levels file.
#' As the DataFrameFactor is a Vector subclass, its R-level metadata can be staged by \code{\link{.processMetadata}}.
#' 
#' @seealso
#' The \code{data_frame_factor} schema from \pkg{alabaster.schemas}.
#' 
#' @author Aaron Lun
#' @examples
#' library(S4Vectors)
#' df <- DataFrame(X=LETTERS[1:5], Y=1:5)
#' out <- DataFrameFactor(df[sample(5, 100, replace=TRUE),,drop=FALSE])
#' 
#' tmp <- tempfile()
#' dir.create(tmp)
#' stageObject(out, tmp, path="test")
#'
#' list.files(file.path(tmp, "test"), recursive=TRUE)
#'
#' @export
#' @rdname stageDataFrameFactor
#' @importFrom utils write.csv
setMethod("stageObject", "DataFrameFactor", function(x, dir, path, child=FALSE, index.name="index", level.name="levels", mcols.name="mcols") {
    dir.create(file.path(dir, path))
    stuff <- levels(x)

    lev.info <- tryCatch({
        info <- .stageObject(stuff, dir, file.path(path, level.name), child=TRUE)
        .writeMetadata(info, dir)
    }, error=function(e) stop("failed to stage underlying DataFrame in a DataFrameFactor\n  - ", e$message))

    path2 <- file.path(path, index.name)
    ofile <- file.path(dir, path2)
    rd <- data.frame(index=as.integer(x))
    if (!is.null(names(x))){ 
        rd <- cbind(row_names=names(x), rd)
    }
    .quickWriteCsv(rd, path=ofile, compression="gzip", row.names=FALSE)

    element_data <- .processMcols(x, dir, path, mcols.name) 

    list(
        `$schema`="data_frame_factor/v1.json",
        path=path2,
        is_child=child,
        factor=list(
            length=length(x),
            names=!is.null(names(x)),
            element_data=element_data,
            compression="gzip"
        ),
        data_frame_factor=list(
            levels=list(resource=lev.info)
        )
    )
})
