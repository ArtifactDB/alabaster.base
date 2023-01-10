#' Create a redirection file
#'
#' Create a redirection to another path in the same staging directory.
#' This is useful for creating short-hand aliases for resources that have inconveniently long paths.
#'
#' @param dir String containing the path to the staging directory.
#' @param src String containing the source path relative to \code{dir}.
#' @param dest String containing the destination path relative to \code{dir}.
#' This may be any path that can also be used in \code{\link{acquireMetadata}}.
#'
#' @return A list of metadata that can be processed by \code{\link{.writeMetadata}}.
#' 
#' @details
#' \code{src} should not correspond to an existing file inside \code{dir}.
#' This avoids ambiguity when attempting to load \code{src} via \code{\link{acquireMetadata}}.
#' Otherwise, it would be unclear as to whether the user wants the file at \code{src} or the redirection target \code{dest}.
#' 
#' \code{src} may correspond to existing directories.
#' This is because directories cannot be used in \code{acquireMetadata}, so no such ambiguity exists.
#'
#' @author Aaron Lun
#' @examples
#' # Staging an example DataFrame:
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#' tmp <- tempfile()
#' dir.create(tmp)
#' info <- stageObject(df, tmp, path="coldata")
#' .writeMetadata(info, tmp)
#'
#' # Creating a redirection:
#' redirect <- .createRedirection(tmp, "foobar", "coldata/simple.csv.gz")
#' .writeMetadata(redirect, tmp)
#'
#' # We can then use this redirect to pull out metadata:
#' info2 <- acquireMetadata(tmp, "foobar")
#' str(info2)
#'
#' @export
#' @rdname createRedirection
.createRedirection <- function(dir, src, dest) {
    json <- paste0(src, ".json")
    if (file.exists(file.path(dir, json))) {
        stop("cannot create a short-hand link over existing file '", json, "'")
    } 

    list(
        path = src,
        `$schema` = "redirection/v1.json",
        redirection = list(
            targets = list(
                list(
                    type = "local",
                    location = dest
                )
            )
        )
    )
}
