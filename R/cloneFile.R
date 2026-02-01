#' Clone an existing file
#'
#' Clone an existing file to a new location.
#' This is typically performed inside \code{\link{saveObject}} methods for objects that contain a reference to a file. 
#'
#' @param src String containing the path to the source file.
#' @param dest String containing the destination file path, typically within the \code{path} supplied to \code{\link{saveObject}}. 
#' @param action String specifying the action to use when cloning \code{src} to \code{dest}.
#' \itemize{
#' \item \code{"copy"}: copy \code{src} to \code{dest}.
#' \item \code{"link"}: create a hard link from \code{src} to \code{dest}.
#' If this fails, we silently fall back to a copy.
#' \item \code{"symlink"}: create a symbolic link from \code{src} to \code{dest}.
#' The symbolic link contains the absolute path to \code{src}, which is useful when \code{dest} might be moved but \code{src} will not.
#' \item \code{"relsymlink"}: create a symbolic link from \code{src} to \code{dest}.
#' Each symbolic link contains the minimal relative path to \code{src},
#' which is useful when both \code{src} and \code{dest} are moved together, e.g., as they are part of the same parent object like a SummarizedExperiment.
#' }
#'
#' @return A new file/link is created at \code{dest}.
#' \code{NULL} is invisibly returned.
#'
#' @author Aaron Lun
#'
#' @examples
#' tmp <- tempfile()
#' write(file=tmp, LETTERS)
#'
#' dest <- tempfile()
#' cloneFile(tmp, dest)
#' readLines(dest)
#'
#' @seealso
#' \code{\link{cloneDirectory}}, to clone entire directories.
#'
#' @export
cloneFile <- function(src, dest, action=c("link", "copy", "symlink", "relsymlink")) {
    action <- match.arg(action)
    destdir <- dirname(dest)
    dir.create(destdir, recursive=TRUE, showWarnings=FALSE)

    if (action == "relsymlink") {
        newtarget <- relative_path_to_src(src, destdir)
        pwd <- getwd()
        on.exit(setwd(pwd))
        setwd(destdir)
        if (!file.symlink(newtarget, basename(dest))) {
            stop("failed to link '", y, "' from '", src, "' to '", dest, "'")
        }

    } else if (action == "link") {
        src <- resolve_symlinks(src) # Resolving any symlinks so that we hard link to the original file.
        if (!suppressWarnings(file.link(src, dest)) && !file.copy(src, dest)) {
            stop("failed to copy or link '", src, "' to '", dest, "'")
        }

    } else if (action == "copy") {
        if (!file.copy(src, dest)) { # file.copy() follows symbolic links, so no need for special treatment here.
            stop("failed to copy '", src, "' to '", dest, "'")
        }

    } else if (action == "symlink") {
        src <- absolutizePath(src)
        if (!file.symlink(src, dest)) {
            stop("failed to symlink '", src, "' to '", dest, "'")
        }
    }
}
