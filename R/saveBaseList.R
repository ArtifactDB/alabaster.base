#' Save a base list to disk
#' 
#' Save a \link{list} or \linkS4class{List} to a JSON or HDF5 file, with extra files created for any of the more complex list elements (e.g., DataFrames, arrays). 
#' This uses the \href{https://github.com/LTLA/uzuki2}{uzuki2} specification to ensure that appropriate types are declared.
#'
#' @param x An ordinary R list, named or unnamed.
#' Alternatively, a \linkS4class{List} to be coerced into a list..
#' @inheritParams saveObject
#' @param list.format String specifying the format in which to save the list.
#' @param ... Further arguments, passed to \code{\link{altSaveObject}} for complex child objects.
#' 
#' @return
#' For the \code{saveObject} method, \code{x} is saved inside \code{dir}.
#' \code{NULL} is invisibly returned.
#'
#' For \code{saveBaseListFormat}; if \code{list.format} is missing, a string containing the current format is returned.
#' If \code{list.format} is supplied, it is used to define the current format, and the \emph{previous} format is returned.
#'
#' @section File formats:
#' If \code{list.format="json.gz"} (default), the list is saved to a Gzip-compressed JSON file (the default).
#' This is an easily parsed format with low storage overhead.
#'
#' If \code{list.format="hdf5"}, \code{x} is saved into a HDF5 file instead.
#' This format is most useful for random access and for preserving the precision of numerical data.
#'
#' @author Aaron Lun
#'
#' @seealso
#' \url{https://github.com/LTLA/uzuki2} for the specification.
#'
#' \code{\link{readBaseList}}, to read the list back into the R session.
#'
#' @examples
#' library(S4Vectors)
#' ll <- list(A=1, B=LETTERS, C=DataFrame(X=1:5))
#'
#' tmp <- tempfile()
#' saveObject(ll, tmp)
#' list.files(tmp, recursive=TRUE)
#' 
#' @export
#' @name saveBaseList
#' @aliases 
#' saveObject,list-method
#' stageObject,list-method
#' stageObject,List-method
#' .saveBaseListFormat
#' @importFrom jsonlite toJSON
setMethod("saveObject", "list", function(x, path, list.format=saveBaseListFormat(), ...) {
    dir.create(path, showWarnings=FALSE)
    dname <- "simple_list"
    write(dname, file=file.path(path, "OBJECT"))

    env <- new.env()
    env$collected <- list()
    args <- list(list.format=list.format, ...)

    if (list.format == "hdf5") {
        fpath <- file.path(path, "list_contents.h5")
        handle <- H5Fcreate(fpath, "H5F_ACC_TRUNC")
        on.exit(H5Fclose(handle), add=TRUE, after=FALSE)

        .transform_list_hdf5(x, dir=NULL, path=path, handle=handle, name=dname, env=env, simplified=TRUE, .version=3, extra=args)

        ghandle <- H5Gopen(handle, dname)
        on.exit(H5Gclose(ghandle), add=TRUE, after=FALSE)
        h5_write_attribute(ghandle, "uzuki_version", "1.3", scalar=TRUE)

    } else {
        formatted <- .transform_list_json(x, dir=NULL, path=path, env=env, simplified=TRUE, .version=2, extra=args)
        formatted$version <- "1.2"

        str <- toJSON(formatted, auto_unbox=TRUE, ident=4, null="null", na="null")
        fpath <- file.path(path, "list_contents.json.gz")
        con <- gzfile(fpath, open="wb")
        write(file=con, str)
        close(con)
    }

    invisible(NULL)
})

#' @export
#' @rdname saveBaseList
setMethod("saveObject", "List", function(x, path, list.format=saveBaseListFormat(), ...) {
    saveObject(as.list(x), path, list.format=list.format, ...)
})

#' @export
#' @rdname saveBaseList
saveBaseListFormat <- (function() {
   mode <- "json.gz"    
   function(list.format) {
       previous <- mode 
       if (missing(list.format)) {
           previous
       } else {
           if (is.null(list.format)) {
               list.format <- "json.gz"
           }
           assign("mode", match.arg(list.format, c("json.gz", "hdf5")), envir=parent.env(environment()))
           invisible(previous)
       }
   }
})()

##################################
########### INTERNALS ############
##################################

#' @importFrom S4Vectors DataFrame
.transform_list_hdf5 <- function(x, dir, path, handle, name, env, simplified, .version, extra) {
    ghandle <- H5Gcreate(handle, name)
    on.exit(H5Gclose(ghandle), add=TRUE, after=FALSE)

    if (is.list(x) && !is.data.frame(x) && !is(x, "POSIXlt")) {
        h5_write_attribute(ghandle, "uzuki_object", "list", scalar=TRUE)
        gdhandle <- H5Gcreate(ghandle, "data")
        on.exit(H5Gclose(gdhandle), add=TRUE, after=FALSE)

        nn <- names(x)
        if (!is.null(nn)) {
            .check_ok_list_names(nn)
            h5_write_vector(ghandle, "names", nn)
        }

        for (i in seq_along(x)) {
            newname <- as.character(i - 1L)
            tryCatch({
                .transform_list_hdf5(x[[i]], dir=dir, path=path, handle=gdhandle, name=newname, env=env, simplified=simplified, .version=.version, extra=extra)
            }, error=function(e) {
                s <- if (is.null(nn)) i else paste0("'", nn[i], "'")
                stop("failed to stage list element ", s, "\n  - ", e$message)
            })
        }

        return(NULL)
    }

    if (is.null(x)) {
        h5_write_attribute(ghandle, "uzuki_object", "nothing", scalar=TRUE)
        return(NULL)
    }

    if (is.null(dim(x))) {
        if (is.factor(x)) {
            h5_write_attribute(ghandle, "uzuki_object", "vector", scalar=TRUE)
            h5_write_attribute(ghandle, "uzuki_type", if (.version == 1 && is.ordered(x)) "ordered" else "factor", scalar=TRUE)

            codes <- as.integer(x) - 1L
            missing.placeholder <- NULL
            if (.version > 1 && anyNA(codes)) {
                missing.placeholder <- -1L
                codes[is.na(codes)] <- missing.placeholder
            }

            local({
                dhandle <- h5_write_vector(ghandle, "data", codes, emit=TRUE)
                on.exit(H5Dclose(dhandle), add=TRUE, after=FALSE)
                if (!is.null(missing.placeholder)) {
                    h5_write_attribute(dhandle, missingPlaceholderName, missing.placeholder, scalar=TRUE)
                }
            })

            h5_write_vector(ghandle, "levels", levels(x))
            if (.version > 1 && is.ordered(x)) {
                h5_write_vector(ghandle, "ordered", 1L, compress=0, scalar=TRUE)
            }
            if (!is.null(names(x))) {
                h5_write_vector(ghandle, "names", names(x))
            }
            return(NULL)

        } else if (!is.null(sltype <- .is_stringlike(x))) {
            h5_write_attribute(ghandle, "uzuki_object", "vector", scalar=TRUE)
            h5_write_attribute(ghandle, "uzuki_type", if (.version == 1) sltype else "string", scalar=TRUE)

            if (.version > 1 && sltype != "string") {
                h5_write_vector(ghandle, "format", sltype, compress=0, scalar=TRUE)
            }

            y <- .sanitize_stringlike(x, sltype)

            missing.placeholder <- NULL
            if (.version > 1) {
                transformed <- transformVectorForHdf5(y, .version=.version)
                y <- transformed$transformed
                missing.placeholder <- transformed$placeholder
            } else if (is.character(y)) {
                if (anyNA(y)) {
                    missing.placeholder <- chooseMissingPlaceholderForHdf5(y, .version=.version)
                    y[is.na(y)] <- missing.placeholder
                }
            }

            local({
                dhandle <- h5_write_vector(ghandle, "data", y, emit=TRUE)
                on.exit(H5Dclose(dhandle), add=TRUE, after=FALSE)
                if (!is.null(missing.placeholder)) {
                    h5_write_attribute(dhandle, missingPlaceholderName, missing.placeholder, scalar=TRUE)
                }
            })
            if (!is.null(names(x))) {
                h5_write_vector(ghandle, "names", names(x))
            }
            return(NULL)

        } else if (is.atomic(x)) {
            coerced <- .remap_atomic_type(x)
            y <- coerced$values

            h5_write_attribute(ghandle, "uzuki_object", "vector", scalar=TRUE)
            h5_write_attribute(ghandle, "uzuki_type", coerced$type, scalar=TRUE)

            missing.placeholder <- NULL
            if (.version > 1) {
                transformed <- transformVectorForHdf5(y, .version=.version)
                y <- transformed$transformed
                missing.placeholder <- transformed$placeholder
            } else {
                if (is.logical(y) && anyNA(y)) {
                    y <- as.integer(y)
                }
            }

            local({
                dhandle <- h5_write_vector(ghandle, "data", y, emit=TRUE)
                on.exit(H5Dclose(dhandle), add=TRUE, after=FALSE)
                if (!is.null(missing.placeholder)) {
                    h5_write_attribute(dhandle, missingPlaceholderName, missing.placeholder, scalar=TRUE)
                }
            })
            if (!is.null(names(x))) {
                h5_write_vector(ghandle, "names", names(x))
            }
            return(NULL)
       }
    }

    # External object fallback.
    h5_write_attribute(ghandle, "uzuki_object", "external", scalar=TRUE)
    n <- length(env$collected)
    h5_write_vector(ghandle, "index", n, compress=0, scalar=TRUE)

    n <- n + 1L
    if (is.data.frame(x)) {
        x <- DataFrame(x, check.names=FALSE)
    }
    tryCatch({
        if (simplified) {
            other.dir <- file.path(path, "other_contents")
            dir.create(other.dir, showWarnings=FALSE)
            do.call(altSaveObject, c(list(x, file.path(other.dir, n)), extra))
            env$collected[[n]] <- TRUE
        } else {
            meta <- altStageObject(x, dir, paste0(path, paste0("/child-", n)), child=TRUE)
            env$collected[[n]] <- list(resource=writeMetadata(meta, dir=dir))
        }
    }, error=function(e) {
        stop("failed to stage '", class(x)[1], "' entry inside a list\n  - ", e$message)
    }) 
}

.is_stringlike <- function(x) {
    if (is(x, "Date")) {
        return("date")
    } else if (.is_datetime(x)) {
        return("date-time")
    } else if (is.character(x)) {
        return("string")
    }
    return(NULL)
}

.sanitize_stringlike <- function(x, type) {
    if (type == "date-time") {
        .sanitize_datetime(x)
    } else {
        as.character(x)
    }
}

.check_ok_list_names <- function(nn) {
    empty <- nn == ""
    if (any(empty)) {
        stop("names of named lists must be non-empty (see element ", which(empty)[1], ")")
    } else if (fail <- anyDuplicated(nn)) {
        stop("names of named lists must be unique (multiple instances of '", nn[fail], "')")
    }
}

.transform_list_json <- function(x, dir, path, env, simplified, .version, extra) {
    if (is.list(x) && !is.data.frame(x)) {
        formatted <- list(type="list")

        nn <- names(x)
        if (!is.null(nn)) {
            .check_ok_list_names(nn)
            formatted$names <- I(nn)
        }

        values <- vector("list", length(x))
        for (i in seq_along(x)) {
            values[[i]] <- tryCatch({
                .transform_list_json(x[[i]], dir=dir, path=path, env=env, simplified=simplified, .version=.version, extra=extra)
            }, error=function(e) {
                s <- if (is.null(nn)) i else paste0("'", nn[i], "'")
                stop("failed to stage list element ", s, "\n  - ", e$message)
            })
        }

        formatted$values <- values
        return(formatted)
    }

    if (is.null(x)) {
        return(list(type="nothing"))
    }

    if (is.null(dim(x))) {
        if (is.factor(x)) {
            formatted <- list(
                type=if (.version == 1 && is.ordered(x)) "ordered" else "factor",
                levels=I(levels(x)),
                values=I(as.integer(x) - 1L)
            )

            if (.version > 1 && is.ordered(x)) {
                formatted$ordered <- TRUE
            }

            formatted <- .add_json_names(x, formatted)
            return(formatted)

        } else if (!is.null(sltype <- .is_stringlike(x))) {
            formatted <- list(
                type=if (.version == 1) sltype else "string",
                values=I(.sanitize_stringlike(x, sltype))
            )

            if (.version > 1 && sltype != "string") {
                formatted$format <- sltype
            }

            formatted <- .add_json_names(x, formatted)
            return(formatted)

        } else if (is.atomic(x)) {
            formatted <- .remap_atomic_type(x)

            y <- formatted$values
            if (is.numeric(y) && !all(is.finite(y))) {
                nan <- is.nan(y)
                inf <- is.infinite(y)
                pos <- y > 0

                has.nan <- any(nan)
                has.inf <- any(inf)
                if (has.nan || has.inf) {
                    y <- as.list(y)
                    if (has.nan) {
                        y[nan] <- list("NaN")
                    }
                    if (has.inf) {
                        y[inf & pos] <- list("Inf")
                        y[inf & !pos] <- list("-Inf")
                    }
                    formatted$values <- y
                }
            }

            formatted$values <- I(formatted$values)
            formatted <- .add_json_names(x, formatted)
            return(formatted)
       }
    }

    # External object fallback.
    n <- length(env$collected)
    formatted <- list(
        type="external",
        index=n
    )

    n <- n + 1L
    if (is.data.frame(x)) {
        x <- DataFrame(x, check.names=FALSE)
    }
    tryCatch({
        if (simplified) {
            other.dir <- file.path(path, "other_contents")
            dir.create(other.dir, showWarnings=FALSE)
            do.call(altSaveObject, c(list(x, file.path(other.dir, n)), extra))
            env$collected[[n]] <- TRUE
        } else {
            meta <- altStageObject(x, dir, paste0(path, paste0("/child-", n)), child=TRUE)
            env$collected[[n]] <- list(resource=writeMetadata(meta, dir=dir))
        }
    }, error=function(e) {
        stop("failed to stage '", class(x)[1], "' entry inside a list\n  - ", e$message)
    }) 

    return(formatted)
}

.add_json_names <- function(x, formatted) {
    if (!is.null(names(x))) {
        formatted$names <- I(names(x))
    }
    formatted
}

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
setMethod("stageObject", "list", function(x, dir, path, child=FALSE, fname="list", .version=NULL) {
    dir.create(file.path(dir, path), showWarnings=FALSE)

    env <- new.env()
    env$collected <- list()
    meta <- list()

    format <- .saveBaseListFormat()
    if (!is.null(format) && format == "hdf5") {
        target <- paste0(path, "/", fname, ".h5")
        fpath <- file.path(dir, target)

        dname <- "contents"
        meta[["$schema"]] <- "hdf5_simple_list/v1.json"
        meta$hdf5_simple_list <- list(group=dname)

        if (is.null(.version)) {
            .version <- 3
        }

        local({
            handle <- H5Fcreate(fpath, "H5F_ACC_TRUNC")
            on.exit(H5Fclose(handle), add=TRUE, after=FALSE)
            .transform_list_hdf5(x, dir=dir, path=path, handle=handle, name=dname, env=env, simplified=FALSE, .version=.version)

            if (.version > 1) {
                ghandle <- H5Gopen(handle, dname)
                on.exit(H5Gclose(ghandle), add=TRUE, after=FALSE)
                h5_write_attribute(ghandle, "uzuki_version", paste0("1.", .version), scalar=TRUE)
            }
        })

        check_list_hdf5(fpath, dname, length(env$collected)) # Check that we did it correctly.

    } else {
        target <- paste0(path, "/", fname, ".json.gz")

        if (is.null(.version)) {
            .version <- 2
        }

        formatted <- .transform_list_json(x, dir=dir, path=path, env=env, simplified=FALSE, .version=.version)

        if (.version > 1) {
            formatted$version <- paste0("1.", .version)
        }

        meta[["$schema"]] <- "json_simple_list/v1.json"
        meta$json_simple_list <- list(compression="gzip")

        str <- toJSON(formatted, auto_unbox=TRUE, ident=4, null="null", na="null")
        fpath <- file.path(dir, target)
        con <- gzfile(fpath, open="wb")
        write(file=con, str)
        close(con)

        check_list_json(fpath, length(env$collected), parallel=TRUE) # Check that we did it correctly.
    }

    extras <- list(
        path=target,
        is_child=child,
        simple_list=list(
            children=env$collected
        )
    )

    c(extras, meta)
})

#' @export
setMethod("stageObject", "List", function(x, dir, path, child=FALSE, fname="list") {
    stageObject(as.list(x), dir, path, child=child, fname=fname)
})

#' @export
.saveBaseListFormat <- function(...) saveBaseListFormat(...)
