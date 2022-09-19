#' How should DataFrames be saved?
#'
#' When \code{\link{stageObject}} is called with a DataFrame (directly or otherwise),
#' should the contents of the \linkS4class{DataFrame} be saved as CSVs, gzipped CSVs, or HDF5?
#'
#' @param format String containing the format to use, namely \code{csv}, \code{csv.gz}, or \code{hdf5}.
#' This may also be \code{NULL} to use the system-specified default (usually CSVs).
#' 
#' @return 
#' If \code{format} is missing, a string containing the current format is returned, or \code{NULL} if no format was specified.
#' If \code{format} is supplied, it is used to define the current format, and the \emph{previous} format is returned.
#'
#' @details
#' \code{\link{stageObject}} for DataFrames will treat a \code{format=NULL} in the same manner as \code{format="csv"}.
#' The distinction exists to allow downstream applications to set their own defaults while still responding to user specification.
#' For example, an application can detect if the existing format is \code{NULL}, and if so, apply another default via \code{.saveDataFrameFormat}.
#' On the other hand, if the format is not \code{NULL}, this is presumably specified by the user explicitly and should be respected by the application.
#'
#' @author Aaron Lun
#'
#' @examples
#' # Updating the global schema getter.
#' (old <- .saveDataFrameFormat())
#'
#' .saveDataFrameFormat("hdf5")
#' .saveDataFrameFormat()
#'
#' # Setting it back.
#' .saveDataFrameFormat(old)
#'
#' @export
#' @rdname saveDataFrameFormat
.saveDataFrameFormat <- (function() {
   cur.env <- new.env()
   cur.env$format <- NULL
   function(format) {
       previous <- cur.env$format
       if (missing(format)) {
           previous
       } else {
           if (!is.null(format)) {
               format <- match.arg(format, c("csv", "csv.gz", "hdf5"))
           }
           cur.env$format <- format
           invisible(previous)
       }
   }
})()
