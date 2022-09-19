#' Stage a base list
#' 
#' Save a \link{list} to a JSON file, with subdirectories created for any of the more complex members.
#' This uses the \pkg{rlist2json} specification to ensure that appropriate types are declared.
#'
#' @inheritParams stageObject
#' @param fname String containing the name of the file to use to save \code{x}. 
#' Note that this should not have a \code{.json} suffix, so as to avoid confusion with the JSON-formatted metadata.
#' 
#' @return
#' A named list containing the metadata for \code{x}, where \code{x} itself is written to a JSON file.
#'
#' @author Aaron Lun
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
#' cat(readLines(file.path(tmp, "stuff/list.gz")), sep="\n")
#' 
#' @export
#' @rdname stageBaseList
#' @importFrom jsonlite toJSON
setMethod("stageObject", "list", function(x, dir, path, child=FALSE, fname="list") {
    dir.create(file.path(dir, path), showWarnings=FALSE)
    env <- new.env()
    env$collected <- list()
    transformed <- .transform_list(x, dir=dir, path=path, env=env)

    contents <- toJSON(transformed, pretty=TRUE, auto_unbox=TRUE, na="null", null="null", digits=NA)
    num.others <- check_list(contents) # check that we did it correctly.
    stopifnot(num.others == length(env$collected))

    target <- file.path(path, paste0(fname, ".gz"))
    handle <- gzfile(file.path(dir, target), "wb")
    on.exit(close(handle))
    write(contents, file=handle)

    list(
        `$schema`="basic_list/v1.json",
        path=target,
        is_child=child,
        basic_list=list(
            children=env$collected,
            compression="gzip"
        )
    )
})

.transform_list <- function(x, dir, path, env) {
    if (is.data.frame(x)) {
        info <- tryCatch({
            .transform_list(as.list(x), dir=dir, path=path, env=env)
        }, error=function(e) stop("failed to stage data frame inside a list\n  - ", e$message))

        out <- list(type="data.frame", columns=info, rows=nrow(x))
        if (!is.null(rownames(x)) && !identical(attr(x, "row.names"), seq_len(nrow(x)))) {
            out$names <- rownames(x)
        }

    } else if (is.list(x)) {
        nn <- names(x)
        if (!is.null(nn)) {
            empty <- nn == ""
            if (any(empty)) {
                stop("names of named lists must be non-empty (see element ", which(empty)[1], ")")
            } else if (fail <- anyDuplicated(nn)) {
                stop("names of named lists must be unique (multiple instances of '", nn[fail], "')")
            }
        }

        out <- x
        for (i in seq_along(x)) {
            tryCatch({
                out[[i]] <- .transform_list(x[[i]], dir=dir, path=path, env=env)
            }, error=function(e) {
                s <- if (is.null(nn)) i else paste0("'", nn[i], "'")
                stop("failed to stage list element ", s, "\n  - ", e$message)
            })
        }

    } else if (is.null(x)) {
        out <- list(type = "nothing")

    } else if (is.array(x)) {
        flattened <- unname(as.vector(x))
        out <- tryCatch({
            .transform_list(flattened, dir=dir, path=path, env=env)
        }, error=function(e) stop("failed to stage array inside a list\n  - ", e$message))
        out$dimensions <- I(dim(x))

        if (!is.null(dimnames(x))) {
            dd <- dimnames(x)
            for (i in seq_along(dd)) {
                if (!is.null(dd[[i]])) {
                    dd[[i]] <- I(dd[[i]])
                }
            }
            out$names <- dd
        }

    } else if (is.factor(x)) {
        ftype <- if (is.ordered(x)) "ordered" else "factor"
        out <- list(type=ftype, values=I(as.character(x)), levels=I(levels(x)))
        out <- .add_names(out, x)

    } else if (is.character(x)) {
        out <- list(type="string", values=I(x))
        out <- .add_names(out, x)

    } else if (is(x, "Date")) {
        out <- list(type="date", values=I(as.character(x)))
        out <- .add_names(out, x)

    } else if (is.atomic(x)) {
        out <- list(type=.remap_type(x), values=I(x))
        out <- .add_names(out, x)

    } else {
        n <- length(env$collected) + 1L
        tryCatch({
            meta <- .stageObject(x, dir, file.path(path, paste0("child-", n)), child=TRUE)
            env$collected[[n]] <- list(resource=.writeMetadata(meta, dir=dir))
        }, error=function(e) stop("failed to stage '", class(x)[1], "' entry inside a list\n  - ", e$message)) 
        out <- list(type="other", index=n-1L)
    }

    out 
}

.add_names <- function(info, x) {
    if (!is.null(names(x))) {
        info$names <- I(names(x))
    }    
    info
}

#' @export
#' @rdname stageBaseList
setMethod("stageObject", "List", function(x, dir, path, child=FALSE, fname="list") {
    stageObject(as.list(x), dir, path, child=child, fname=fname)
})

