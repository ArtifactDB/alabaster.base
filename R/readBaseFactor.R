#' Read a factor from disk
#'
#' Read a base R \link{factor} from its on-disk representation.
#'
#' @param path String containing a path to a directory, itself created with the \code{\link{saveObject}} method for factors.
#' @param ... Further arguments, ignored.
#'
#' @return The vector described by \code{info}, possibly with names.
#'
#' @seealso
#' \code{"\link{stageObject,factor-method}"}, for the staging method.
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
    on.exit(H5Fclose(fhandle))

    host <- "string_factor"
    ghandle <- H5Gopen(fhandle, host)
    on.exit(H5Gclose(ghandle), add=TRUE, after=FALSE)

    attrs <- h5readAttributes(fhandle, host)
    codes <- .simple_read_codes(fhandle, host)
    levels <- as.vector(h5read(fhandle, paste0(host, "/levels")))
    output <- factor(levels[codes], levels=levels, ordered=isTRUE(attrs$ordered > 0))

    if (h5exists(ghandle, "names")) {
        names(output) <- as.vector(h5read(ghandle, "names"))
    }
    output 
}

.simple_read_codes <- function(fhandle, host) {
    code.name <- paste0(host, "/codes")
    codes <- as.vector(h5read(fhandle, code.name))
    code.attrs <- h5readAttributes(fhandle, code.name)
    codes <- .h5cast(codes, code.attrs, type="integer")
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
