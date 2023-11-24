#' Read a factor from disk
#'
#' Read a base R \link{factor} from its on-disk representation.
#'
#' @param path String containing a path to a directory, itself created with the \code{\link{saveObject}} method for factors.
#' @param ... Further arguments, ignored.
#'
#' @return 
#' The vector described by \code{info}. 
#'
#' @seealso
#' \code{"\link{saveObject,factor-method}"}, for the staging method.
#'
#' @author Aaron Lun
#' 
#' @examples
#' tmp <- tempfile()
#' saveObject(factor(letters[1:10], letters), tmp)
#' readBaseFactor(tmp)
#' 
#' @export
#' @aliases loadBaseFactor
readBaseFactor <- function(path, ...) {
    fpath <- file.path(path, "contents.h5")
    fhandle <- H5Fopen(fpath)
    on.exit(H5Fclose(fhandle), add=TRUE, after=FALSE)

    host <- "string_factor"
    ghandle <- H5Gopen(fhandle, host)
    on.exit(H5Gclose(ghandle), add=TRUE, after=FALSE)

    codes <- .simple_read_codes(ghandle)
    levels <- h5_read_vector(ghandle, "levels")
    ordered <- h5_read_attribute(ghandle, "ordered", check=TRUE, default=NULL)
    output <- factor(levels[codes], levels=levels, ordered=isTRUE(ordered > 0L))

    if (h5_object_exists(ghandle, "names")) {
        names(output) <- h5_read_vector(ghandle, "names")
    }
    output 
}

.simple_read_codes <- function(handle, name="codes") {
    chandle <- H5Dopen(handle, name)
    on.exit(H5Dclose(chandle), add=TRUE, after=FALSE)
    codes <- H5Dread(chandle, drop=TRUE)
    missing.placeholder <- h5_read_attribute(chandle, missing_placeholder_name, check=TRUE, default=NULL)
    codes <- h5_cast(codes, expected.type="integer", missing.placeholder=missing.placeholder)
    codes + 1L
}

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
loadBaseFactor <- function(info, project, ...) {
    fpath <- acquireFile(project, info$path)
    meta <- info$factor

    df <- read.csv3(fpath, compression=meta$compression, nrows=meta$length)
    codes <- df[,ncol(df)] + 1L

    smeta <- info$string_factor
    level_meta <- acquireMetadata(project, smeta$levels$resource$path)
    levels <- altLoadObject(level_meta, project=project)

    output <- factor(levels[codes], levels=levels, ordered=isTRUE(smeta$ordered))
    if (isTRUE(meta$names)) {
        names(output) <- df[,1]
    }
    output 
}
