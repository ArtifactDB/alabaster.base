#' Stage a DataFrameFactor object
#'
#' Stage a \linkS4class{DataFrameFactor} object, a generalization of the base factor for \linkS4class{DataFrame} levels.
#'
#' @param x A \linkS4class{DataFrameFactor} object.
#' @inheritParams saveObject 
#' @param ... Further arguments, to pass to internal \code{\link{altSaveObject}} calls.
#'
#' @return 
#' \code{x} is saved to an on-disk representation inside \code{path}.
#' 
#' @author Aaron Lun
#' @examples
#' library(S4Vectors)
#' df <- DataFrame(X=LETTERS[1:5], Y=1:5)
#' out <- DataFrameFactor(df[sample(5, 100, replace=TRUE),,drop=FALSE])
#' 
#' tmp <- tempfile()
#' saveObject(out, tmp)
#' list.files(tmp, recursive=TRUE)
#'
#' @export
#' @rdname saveDataFrameFactor
#' @aliases stageObject,DataFrameFactor-method
setMethod("saveObject", "DataFrameFactor", function(x, path, ...) {
    dir.create(path)
    ofile <- file.path(path, "contents.h5")

    fhandle <- H5Fcreate(ofile, "H5F_ACC_TRUNC")
    on.exit(H5Fclose(fhandle), add=TRUE, after=FALSE)
    ghandle <- H5Gcreate(fhandle, "data_frame_factor")
    on.exit(H5Gclose(ghandle), add=TRUE, after=FALSE)
    h5_write_attribute(ghandle, "version", "1.0", scalar=TRUE)

    .simple_save_codes(ghandle, x)
    stuff <- levels(x)
    altSaveObject(stuff, file.path(path, "levels"), ...)

    saveMetadata(x, 
        mcols.path=file.path(path, "element_annotations"),
        metadata.path=file.path(path, "other_annotations"),
        ...
    )

    write("data_frame_factor", file=file.path(path, "OBJECT"))
})

.anyDuplicated_fallback <- function(path, ...) {
    anyDuplicated(readObject(path, ...))
}

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
#' @importFrom utils write.csv
setMethod("stageObject", "DataFrameFactor", function(x, dir, path, child=FALSE, index.name="index", level.name="levels", mcols.name="mcols") {
    dir.create(file.path(dir, path))
    stuff <- levels(x)

    lev.info <- tryCatch({
        info <- altStageObject(stuff, dir, paste0(path, "/", level.name), child=TRUE)
        writeMetadata(info, dir)
    }, error=function(e) stop("failed to stage underlying DataFrame in a DataFrameFactor\n  - ", e$message))

    path2 <- paste0(path, "/", index.name)
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
