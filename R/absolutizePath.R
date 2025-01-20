#' Make an absolute file path
#'
#' Create an absolute file path from a relative file path.
#' All processing is purely lexical; the path itself does not have to exist on the filesystem.
#'
#' @param path String containing an absolute or relative file path.
#'
#' @return An absolute file path corresponding to \code{path}, or \code{path} itself if it was already absolute.
#'
#' @author Aaron Lun
#'
#' @examples
#' absolutizePath("alpha")
#' absolutizePath("/alpha/bravo")
#' 
#' @export
absolutizePath <- function(path) {
    components <- path_components(path)
    if (components[1] == ".") {
        file.path(getwd(), path)
    } else {
        path
    }
}

path_components <- function(path) {
    output <- character()
    while (TRUE) {
        base <- basename(path)
        dpath <- dirname(path)
        output <- c(base, output)
        if (dpath == path) {
            break
        }
        path <- dpath
    }
    output
}

