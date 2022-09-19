#' Create a redirection file
#'
#' Create a redirection to another path in the same staging directory.
#' This is useful for creating short-hand aliases for resources that have inconveniently long paths.
#'
#' @param dir String containing the path to the staging directory.
#' @param src String containing the source path relative to \code{dir}.
#' @param dest String containing the destination path relative to \code{dir}.
#'
#' @return A list of metadata that can be processed by \code{\link{.writeMetadata}}.
#'
#' @author Aaron Lun
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
