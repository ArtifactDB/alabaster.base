#' Utilities to read and save the object file
#'
#' The OBJECT file inside each directory provides some high-level metadata of the object represented by that directory.
#' It is guaranteed to have a \code{type} property that specifies the object type;
#' individual objects may add their own information to this file.
#' These methods are intended for developers to easily read and load information in the OBJECT file.
#'
#' @param path Path to the directory representing an object.
#' @param type String specifying the type of the object.
#' @param extra Named list containing extra metadata to be written to the OBJECT file in \code{path}.
#' None of the names should be \code{"type"}.
#'
#' @return \code{readObjectFile} returns a named list of metadata for \code{path}.
#'
#' \code{saveObjectFile} saves \code{metadata} to the OBJECT file inside \code{path}
#'
#' @author Aaron Lun
#' 
#' @examples
#' tmp <- tempfile()
#' dir.create(tmp)
#' saveObjectFile(tmp, "foo", list(bar=list(version="1.0")))
#' readObjectFile(tmp)
#' 
#' @export
#' @importFrom jsonlite fromJSON
readObjectFile <- function(path) {
    fromJSON(file.path(path, "OBJECT"), simplifyVector=FALSE)
}

#' @export
#' @rdname readObjectFile
#' @importFrom jsonlite toJSON
saveObjectFile <- function(path, type, extra=list()) {
    metadata <- c(list(type=type), extra)
    write(toJSON(metadata, auto_unbox=TRUE, pretty=4), file=file.path(path, "OBJECT"))
}
