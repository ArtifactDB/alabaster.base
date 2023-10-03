#' Stage a base list
#' 
#' Save a \link{list} or \linkS4class{List} to a JSON or HDF5 file, with external subdirectories created for any of the more complex list elements (e.g., DataFrames, arrays). 
#' This uses the \href{https://github.com/LTLA/uzuki2}{uzuki2} specification to ensure that appropriate types are declared.
#'
#' @inheritParams stageObject
#' @param fname String containing the name of the file to use to save \code{x}. 
#' Note that this should not have a \code{.json} suffix, so as to avoid confusion with the JSON-formatted metadata.
#' @param .version Internal use only.
#' 
#' @return
#' A named list containing the metadata for \code{x}, where \code{x} itself is written to a JSON file.
#'
#' @section File formats:
#' If \code{\link{.saveBaseListFormat}() == "json.gz"}, the list is saved to a Gzip-compressed JSON file (the default).
#' This is an easily parsed format with low storage overhead.
#'
#' If \code{\link{.saveBaseListFormat}() == "hdf5"}, \code{x} is saved into a HDF5 file instead.
#' This format is most useful for random access and for preserving the precision of numerical data.
#'
#' @author Aaron Lun
#'
#' @seealso
#' \url{https://github.com/LTLA/uzuki2} for the specification.
#'
#' The \code{json_simple_list} and \code{hdf5_simple_list} schemas from the \pkg{alabaster.schemas} package.
#'
#' @examples
#' library(S4Vectors)
#' ll <- list(A=1, B=LETTERS, C=DataFrame(X=1:5))
#'
#' tmp <- tempfile()
#' dir.create(tmp)
#' stageObject(ll, tmp, path="stuff")
#'
#' list.files(tmp, recursive=TRUE)
#' 
#' @export
#' @rdname stageBaseList
#' @importFrom rhdf5 h5createFile
#' @importFrom jsonlite toJSON
setMethod("stageObject", "list", function(x, dir, path, child=FALSE, fname="list", .version=2) {
    dir.create(file.path(dir, path), showWarnings=FALSE)

    env <- new.env()
    env$collected <- list()
    meta <- list()

    format <- .saveBaseListFormat()
    if (!is.null(format) && format == "hdf5") {
        target <- paste0(path, "/", fname, ".h5")
        fpath <- file.path(dir, target)
        h5createFile(fpath)

        dname <- "contents"
        meta[["$schema"]] <- "hdf5_simple_list/v1.json"
        meta$hdf5_simple_list <- list(group=dname)

        .transform_list_hdf5(x, dir=dir, path=path, fpath=fpath, name=dname, env=env, .version=.version)
        if (.version > 1) {
            .label_hdf5_group(fpath, dname, uzuki_version="1.1")
        }

        check_list_hdf5(fpath, dname, length(env$collected)) # Check that we did it correctly.

    } else {
        target <- paste0(path, "/", fname, ".json.gz")
        formatted <- .transform_list_json(x, dir=dir, path=path, env=env, .version=.version)
        if (.version > 1) {
            formatted$version <- "1.1"
        }
        str <- toJSON(formatted, auto_unbox=TRUE, ident=4, null="null", na="null")

        fpath <- file.path(dir, target)
        con <- gzfile(fpath, open="wb")
        write(file=con, str)
        close(con)

        check_list_json(fpath, length(env$collected), parallel=TRUE) # Check that we did it correctly.
        meta[["$schema"]] <- "json_simple_list/v1.json"
        meta$json_simple_list <- list(compression="gzip")
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

#' @importFrom S4Vectors DataFrame
#' @importFrom rhdf5 h5createGroup h5write
.transform_list_hdf5 <- function(x, dir, path, fpath, name, env, .version) {
    h5createGroup(fpath, name)

    if (is.list(x) && !is.data.frame(x) && !is(x, "POSIXlt")) {
        .label_hdf5_group(fpath, name, uzuki_object="list")
        h5createGroup(fpath, paste0(name, "/data"))

        nn <- names(x)
        if (!is.null(nn)) {
            .check_ok_list_names(nn)
            h5write(nn, fpath, paste0(name, "/names"))
        }

        for (i in seq_along(x)) {
            newname <- paste0(name, "/data/", i - 1L)
            tryCatch({
                .transform_list_hdf5(x[[i]], dir=dir, path=path, fpath=fpath, name=newname, env=env, .version=.version)
            }, error=function(e) {
                s <- if (is.null(nn)) i else paste0("'", nn[i], "'")
                stop("failed to stage list element ", s, "\n  - ", e$message)
            })
        }

        return(NULL)
    }

    if (is.null(x)) {
        .label_hdf5_group(fpath, name, uzuki_object="nothing")
        return(NULL)
    }

    if (is.null(dim(x))) {
        if (is.factor(x)) {
            .label_hdf5_group(fpath, name, 
                uzuki_object="vector",
                uzuki_type=if (.version == 1 && is.ordered(x)) "ordered" else "factor"
            )

            dataname <- paste0(name, "/data")
            h5write(as.integer(x) - 1L, fpath, dataname)
            if (.version > 1 && anyNA(x)) {
                .add_missing_value_placeholder(fpath, dataname, -1L)
            }

            h5write(levels(x), fpath, paste0(name, "/levels"))
            if (.version > 1 && is.ordered(x)) {
                write_integer_scalar(fpath, name, "ordered", 1)
            }

            .add_hdf5_names(x, fpath, name)
            return(NULL)

        } else if (!is.null(sltype <- .is_stringlike(x))) {
            .label_hdf5_group(fpath, name, 
                uzuki_object="vector",
                uzuki_type=if (.version == 1) sltype else "string"
            )

            if (.version > 1 && sltype != "string") {
                write_string_scalar(fpath, name, "format", sltype)
            }

            y <- .sanitize_stringlike(x, sltype)
            missing.placeholder <- NULL
            if (anyNA(y)) {
                missing.placeholder <- .chooseMissingStringPlaceholder(x)
                y[is.na(y)] <- missing.placeholder
            }

            dname <- paste0(name, "/data")
            h5write(y, fpath, dname)
            if (!is.null(missing.placeholder)) {
                .addMissingStringPlaceholderAttribute(fpath, dname, missing.placeholder)
            }
            .add_hdf5_names(x, fpath, name)
            return(NULL)

        } else if (is.atomic(x)) {
            coerced <- .remap_atomic_type(x)

            .label_hdf5_group(fpath, name, 
                uzuki_object="vector",
                uzuki_type=coerced$type
            )

            y <- coerced$values

            placeholder <- NULL
            if (.version > 1) {
                if (is.logical(y)) {
                    # Force the use of a regular integer to avoid confusion.
                    y <- as.integer(y)
                    if (anyNA(y)) {
                        placeholder <- -1L
                        y[is.na(y)] <- placeholder
                    }
                } else if (anyNA(y)) {
                    placeholder <- as(NA, storage.mode(y))
                }
            } else {
                if (is.logical(y) && anyNA(y)) {
                    y <- as.integer(y)
                }
            }

            dataname <- paste0(name, "/data")
            h5write(y, fpath, dataname)
            .add_hdf5_names(x, fpath, name)
            if (.version > 1 && !is.null(placeholder)) {
                .add_missing_value_placeholder(fpath, dataname, placeholder)
            }

            return(NULL)
       }
    }

    # External object fallback.
    .label_hdf5_group(fpath, name, uzuki_object="external")
    n <- length(env$collected)
    write_integer_scalar(fpath, name, "index", n)

    n <- n + 1L
    if (is.data.frame(x)) {
        x <- DataFrame(x, check.names=FALSE)
    }
    tryCatch({
        meta <- altStageObject(x, dir, paste0(path, paste0("/child-", n)), child=TRUE)
        env$collected[[n]] <- list(resource=writeMetadata(meta, dir=dir))
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

#' @importFrom rhdf5 H5Fopen H5Fclose H5Gopen H5Gclose h5writeAttribute 
.label_hdf5_group <- function(file, name, ...) {
    fhandle <- H5Fopen(file)
    on.exit(H5Fclose(fhandle), add=TRUE)
    ghandle <- H5Gopen(fhandle, name)
    on.exit(H5Gclose(ghandle), add=TRUE)

    attrs <- list(...)
    for (a in names(attrs)) {
        h5writeAttribute(attrs[[a]], h5obj=ghandle, name=a, asScalar=TRUE)
    }
}

#' @importFrom rhdf5 H5Fopen H5Fclose h5writeAttribute H5Dopen H5Dclose
.add_missing_value_placeholder <- function(file, name, value) {
    fhandle <- H5Fopen(file)
    on.exit(H5Fclose(fhandle), add=TRUE)
    dhandle <- H5Dopen(fhandle, name)
    on.exit(H5Dclose(dhandle), add=TRUE)
    h5writeAttribute(value, h5obj=dhandle, name="missing-value-placeholder", asScalar=TRUE)
}

#' @importFrom rhdf5 H5Fopen H5Fclose H5Gopen H5Gclose h5writeAttribute H5Dcreate H5Dclose H5Dwrite H5Screate H5Sclose
.write_scalar_dataset <- function(file, parent, name, value, datatype) {
    fhandle <- H5Fopen(file)
    on.exit(H5Fclose(fhandle), add=TRUE)
    ghandle <- H5Gopen(fhandle, parent)
    on.exit(H5Gclose(ghandle), add=TRUE)

    shandle <- H5Screate("H5S_SCALAR")
    on.exit(H5Sclose(shandle), add=TRUE)
    dhandle <- H5Dcreate(ghandle, name, dtype_id=datatype, h5space=shandle)
    on.exit(H5Dclose(dhandle), add=TRUE)

    H5Dwrite(dhandle, value) 
}

#' @importFrom rhdf5 h5write
.add_hdf5_names <- function(x, fpath, name) {
    if (!is.null(names(x))) {
        h5write(names(x), fpath, paste0(name, "/names"))
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

.transform_list_json <- function(x, dir, path, env, .version) {
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
                .transform_list_json(x[[i]], dir=dir, path=path, env=env, .version=.version)
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
        meta <- altStageObject(x, dir, paste0(path, paste0("/child-", n)), child=TRUE)
        env$collected[[n]] <- list(resource=writeMetadata(meta, dir=dir))
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

#' @export
#' @rdname stageBaseList
setMethod("stageObject", "List", function(x, dir, path, child=FALSE, fname="list") {
    stageObject(as.list(x), dir, path, child=child, fname=fname)
})

