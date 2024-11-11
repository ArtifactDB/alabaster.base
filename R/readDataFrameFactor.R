#' Read a DataFrame factor from disk
#'
#' Read a \link[S4Vectors]{DataFrameFactor} from its on-disk representation.
#' This is usually not directly called by users, but is instead called by dispatch in \code{\link{readObject}}.
#'
#' @param path String containing a path to a directory, itself created with the \code{\link{saveObject}} method for \link[S4Vectors]{DataFrameFactor}s.
#' @param metadata Named list containing metadata for the object, see \code{\link{readObjectFile}} for details.
#' @param ... Further arguments to pass to internal \code{\link{altSaveObject}} calls.
#'
#' @return A \link[S4Vectors]{DataFrameFactor} represented by \code{path}.
#'
#' @author Aaron Lun
#'
#' @seealso
#' \code{"\link{saveObject,DataFrameFactor-method}"}, for the staging method.
#'
#' @examples
#' library(S4Vectors)
#' df <- DataFrame(X=LETTERS[1:5], Y=1:5)
#' out <- DataFrameFactor(df[sample(5, 100, replace=TRUE),,drop=FALSE])
#' 
#' tmp <- tempfile()
#' saveObject(out, tmp)
#' readObject(tmp)
#'
#' @export
#' @aliases loadDataFrameFactor
#' @importFrom S4Vectors DataFrameFactor
readDataFrameFactor <- function(path, metadata, ...) {
    fpath <- file.path(path, "contents.h5")
    fhandle <- H5Fopen(fpath, flags="H5F_ACC_RDONLY")
    on.exit(H5Fclose(fhandle), add=TRUE, after=FALSE)

    host <- "data_frame_factor"
    ghandle <- H5Gopen(fhandle, host)
    on.exit(H5Gclose(ghandle), add=TRUE, after=FALSE)

    codes <- .simple_read_codes(ghandle)
    levels <- altReadObject(file.path(path, "levels"), ...)
    output <- DataFrameFactor(index=codes, levels=levels)

    if (h5_object_exists(ghandle, "names")) {
        names(output) <- h5_read_vector(ghandle, "names")
    }

    readMetadata(
        output,
        metadata.path=file.path(path, "other_annotations"),
        mcols.path=file.path(path, "element_annotations"),
        ...
    )
}

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
loadDataFrameFactor <- function(info, project, ...) {
    lev.info <- acquireMetadata(project, info$data_frame_factor$levels$resource$path)
    levels <- altLoadObject(lev.info, project=project)

    path <- acquireFile(project, info$path)
    has.names <- isTRUE(info$factor$names)
    idx <- .quickReadCsv(path, 
         c(index="integer"), 
         row.names=has.names, 
         compression=info$factor$compression,
         expected.nrows=info$factor$length
    )

    indexes <- idx$index
    if (has.names) {
        names(indexes) <- rownames(idx)
    }

    DataFrameFactor(index=indexes, levels=levels)
}
