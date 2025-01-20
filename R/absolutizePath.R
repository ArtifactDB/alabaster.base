#' Make an absolute file path
#'
#' Create an absolute file path from a relative file path.
#' All processing is purely lexical; the path itself does not have to exist on the filesystem.
#'
#' @param path String containing an absolute or relative file path.
#'
#' @return An absolute file path corresponding to \code{path}.
#' This is cleaned to remove \code{..}, \code{.} and \code{~} components.
#'
#' @author Aaron Lun
#'
#' @examples
#' absolutizePath("alpha")
#' absolutizePath("../alpha")
#' absolutizePath("../../alpha/./bravo")
#' absolutizePath("/alpha/bravo")
#' 
#' @export
absolutizePath <- function(path) {
    tmp <- path
    decomposed <- decompose_path(path)

    if (decomposed$relative) {
        # getwd() always returns an absolute path,
        # so the combined components will also be absolute.
        wd <- decompose_path(getwd())
        root <- wd$root
        comp <- c(wd$components, decomposed$components)
    } else {
        root <- decomposed$root
        comp <- decomposed$components
    }

    # Cleaning the path.
    cleaned <- character()
    for (x in comp) {
        if (x == "..") {
            cleaned <- head(cleaned, -1)
        } else if (x != ".") {
            cleaned <- c(cleaned, x)
        }
    }

    paste0(root, do.call(file.path, as.list(cleaned)))
}
