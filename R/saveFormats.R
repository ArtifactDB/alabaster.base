format.env <- new.env()
format.env$DataFrame <- NULL

#' Choose the format for certain objects
#'
#' Alter the format used to save DataFrames or base lists in their respective \code{\link{stageObject}} methods.
#'
#' @param format String containing the format to use.
#' \itemize{
#' \item For \code{saveDataFrameFormat}, this may be \code{"csv"}, \code{"csv.gz"} (default) or \code{"hdf5"}.
#' \item For \code{saveBaseListFormat}, this may be \code{"json.gz"} (default) or \code{"hdf5"}.
#' }
#' Alternatively \code{NULL}, to use the default format.
#' 
#' @return 
#' If \code{format} is missing, a string containing the current format is returned, or \code{NULL} to use the default format.
#'
#' If \code{format} is supplied, it is used to define the current format, and the \emph{previous} format is returned.
#'
#' @details
#' \code{\link{stageObject}} methods will treat a \code{format=NULL} in the same manner as the default format.
#' The distinction exists to allow downstream applications to set their own defaults while still responding to user specification.
#' For example, an application can detect if the existing format is \code{NULL}, and if so, apply another default via \code{.saveDataFrameFormat}.
#' On the other hand, if the format is not \code{NULL}, this is presumably specified by the user explicitly and should be respected by the application.
#'
#' @author Aaron Lun
#'
#' @examples
#' (old <- .saveDataFrameFormat())
#'
#' .saveDataFrameFormat("hdf5")
#' .saveDataFrameFormat()
#'
#' # Setting it back.
#' .saveDataFrameFormat(old)
#'
#' @name saveFormats
NULL

#' @export
#' @rdname saveFormats
.saveDataFrameFormat <- function(format) {
    .save_format(format, "DataFrame", c("csv", "csv.gz", "hdf5"))
}

#' @export
#' @rdname saveFormats
.saveBaseListFormat <- function(format) {
    .save_format(format, "BaseList", c("json.gz", "hdf5"))
}

.save_format <- function(format, mode, choices) {
   previous <- format.env[[mode]]
   if (missing(format)) {
       previous
   } else {
       if (!is.null(format)) {
           format <- match.arg(format, choices)
       }
       format.env[[mode]] <- format
       invisible(previous)
   }
}
