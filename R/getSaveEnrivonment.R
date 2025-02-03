#' Track the environment used for saving objects
#'
#' Utilities to write, load and access the R environment used by \code{\link{saveObject}} for any given object.
#'
#' @param path String containing the path to a directory representing an object,
#' same as that used by \code{\link{saveObject}} and \code{\link{readObject}}.
#' @param info Named list containing information about the environment used to save each object.
#' If \code{NULL}, this is created by calling \code{formatSaveEnvironment}.
#' @param use Logical scalar specifying whether to use a save environment during reading/saving of objects.
#' 
#' @return
#' \code{getSaveEnvironment} returns a named list describing the environment used to save the \dQuote{current} object (see Details).
#' The list should have a \code{type} field specifying the type of environment, e.g., \code{"R"}.
#' For objects created by \code{\link{saveObject}}, this will typically have the same format as the list returned by \code{formatSaveEnvironment}.
#' Alternatively, \code{NULL} is returned if \code{useSaveEnvironment} is set to \code{FALSE} or no environment information was recorded for the current object.
#'
#' \code{formatSaveEnvironment} returns a named list containing the current R environment, derived from the \code{\link[utils]{sessionInfo}}.
#' This records the R \code{version}, the \code{platform} in which R is running, and the versions of all \code{packages} as a named list.
#'
#' If \code{use} is not supplied, \code{useSaveEnvironment} returns a logical scalar indicating whether to use the save environment information.
#' If \code{use} is supplied, it is used to define the save environment usage policy, and the previous setting of this value is invisibly returned.
#'
#' \code{registerSaveEnvironment} registers the current environment information in memory so that it can be returned by \code{getSaveEnvironment}.
#' It returns a list containing a \code{restore} function that should be called \code{\link{on.exit}} to (i) restore the previous environment information;
#' and a \code{write} function that accepts a \code{path} to a directory in which to create an \code{_environment.json} file with the environment information.
#' Both functions are no-ops if \code{useSaveEnvironment} is set to \code{FALSE} or if a save environment has already been registered.
#'
#' \code{loadSaveEnvironment} loads the environment information from a \code{_environment.json} file in \code{path}.
#' It also registers the environment information in memory so that it is returned when \code{getSaveEnvironment} is called.
#' It returns a function that should be called \code{\link{on.exit}} to restore the previous environment information. 
#' This function is a no-op if \code{useSaveEnvironment} is set to \code{FALSE}, or if the environment information is not parsable (in which case a warning will be emitted).
#'
#' @details
#' When saving an object, \pkg{\link{saveObject}} will automatically record some details about the current R environment. 
#' This facilitates trouble-shooting and provides some opportunities for corrective measures if any bugs are found in older \code{saveObject} methods.
#' Information about the save environment is stored in an \code{_environment.json} file inside the directory containing the object.
#' Subdirectories for child objects may also have separate \code{_environment.json} files (e.g., if they were created in a different environment),
#' otherwise it is assumed that they inherit the save environment from the parent object.
#'
#' Application or extension developers are expected to call \code{getSaveEnvironment} from inside a loading function used by \code{\link{readObject}} or \code{\link{altReadObject}}.
#' This wil return the save environment that was used for the \dQuote{current} object, i.e., the object that was previously saved at \code{path}.
#' By accessing the historical save environment, developers can check if buggy versions of the corresponding \code{saveObject} or \code{altSaveObject} methods were used.
#' Appropriate corrective measures can then be applied to recover the correct object, warn users, etc.
#' \code{getSaveEnvironment} can also be called inside \code{saveObject} or \code{altSaveObject} methods, in which case the current object is the one being saved.
#' 
#' In most cases, \code{registerSaveEnvironment} does not need to be explicitly called by end-users or developers.
#' It is automatically executed by the top-level calls to the \code{\link{saveObject}} or \code{\link{altSaveObject}} generics.
#' Methods can simply call \code{\link{getSaveEnvironment}} to access the save environment information.
#' Similarly, \code{loadSaveEnvironment} does not usually need to be explicitly called by end-users or developers,
#' as it is automatically executed by each \code{\link{readObject}} or \code{\link{altReadObject}} call.
#' Individual reader functions can simply call \code{\link{getSaveEnvironment}} to access the save environment information.
#'
#' Tracking of the save environment can be disabled by setting \code{useSaveEnvironment(FALSE)}.
#'
#' @author Aaron Lun
#'
#' @examples
#' str(formatSaveEnvironment())
#'
#' prev <- useSaveEnvironment(TRUE)
#' tmp <- tempfile()
#' dir.create(tmp)
#'
#' wfun <- registerSaveEnvironment(tmp)
#' getSaveEnvironment()
#' wfun$restore()
#'
#' useSaveEnvironment(prev)
#'
#' @export
getSaveEnvironment <- function() {
    if (useSaveEnvironment()) {
        save.env$environment
    } else {
        NULL
    }
}

save.env <- new.env()
save.env$use <- TRUE
save.env$environment <- NULL

#' @export
#' @rdname getSaveEnvironment
#' @importFrom utils sessionInfo
formatSaveEnvironment <- function() {
    info <- sessionInfo()
    list(
        type="R",
        version=paste0(info$R.version$major, ".", info$R.version$minor),
        platform=info$platform,
        packages=c(
            lapply(info$otherPkgs, function(x) x$Version),
            lapply(info$loadedOnly, function(x) x$Version)
        )
    )
}

#' @export
#' @rdname getSaveEnvironment
useSaveEnvironment <- function(use) {
    old <- save.env$use
    if (missing(use)) {
        return(old)
    } else {
        save.env$use <- use
        return(invisible(old))
    }
}

#' @export
#' @rdname getSaveEnvironment
#' @importFrom jsonlite toJSON
registerSaveEnvironment <- function(info=NULL) {
    if (!is.null(getSaveEnvironment()) || !useSaveEnvironment()) {
        return(list(
            restore = function() {},
            write = function(path) {}
        ))
    }

    if (is.null(info)) {
        info <- formatSaveEnvironment()
    }

    previous <- save.env$environment
    save.env$environment <- info
    list(
        restore = function() {
            save.env$environment <- previous
        },
        write = function(path) {
            # We do this here as 'path' isn't created yet.
            write(file=.get_environment_path(path), toJSON(info, pretty=4, auto_unbox=TRUE))
        }
    )
}

.get_environment_path <- function(path) {
    file.path(path, "_environment.json")
}

#' @export
#' @rdname getSaveEnvironment
#' @importFrom jsonlite fromJSON
loadSaveEnvironment <- function(path) {
    candidate <- .get_environment_path(path)
    if (!useSaveEnvironment() || !file.exists(candidate)) {
        return(function(){})
    }

    current <- try(fromJSON(candidate, simplifyVector=FALSE), silent=TRUE)
    if (is(current, "try-error")) {
        warning("failed to read environment information at '", path, "', ", attr(current, "condition")$message)
        return(function(){})
    }

    previous <- save.env$environment
    save.env$environment <- current
    function() {
        save.env$environment <- previous
    }
}
