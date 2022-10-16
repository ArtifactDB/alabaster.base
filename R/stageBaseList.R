#' Stage a base list
#' 
#' Save a \link{list} to a HDF5 file, with subdirectories created for any of the more complex members.
#' This uses the uzuki2 specification to ensure that appropriate types are declared.
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
#' @seealso
#' \url{https://github.com/LTLA/uzuki2} for the specification.
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
setMethod("stageObject", "list", function(x, dir, path, child=FALSE, fname="list") {
    dir.create(file.path(dir, path), showWarnings=FALSE)

    target <- paste0(path, "/", fname, ".h5")
    fpath <- file.path(dir, target)
    h5createFile(fpath)
    dname <- "contents"
 
    env <- new.env()
    env$collected <- list()
    transformed <- .transform_list(x, dir=dir, path=path, fpath=fpath, name=dname, env=env)

    # Check that we did it correctly.
    check_list(fpath, dname, length(env$collected)) 

    list(
        `$schema`="hdf5_simple_list/v1.json",
        path=target,
        is_child=child,
        simple_list=list(
            children=env$collected
        ),
        hdf5_simple_list=list(
            group=dname
        )
    )
})

#' @importFrom S4Vectors DataFrame
#' @importFrom rhdf5 h5createGroup h5write
.transform_list <- function(x, dir, path, fpath, name, env) {
    h5createGroup(fpath, name)

    if (is.list(x) && !is.data.frame(x)) {
        .label_group(fpath, name, uzuki_object="list")
        h5createGroup(fpath, paste0(name, "/data"))

        nn <- names(x)
        if (!is.null(nn)) {
            empty <- nn == ""
            if (any(empty)) {
                stop("names of named lists must be non-empty (see element ", which(empty)[1], ")")
            } else if (fail <- anyDuplicated(nn)) {
                stop("names of named lists must be unique (multiple instances of '", nn[fail], "')")
            }
            h5write(nn, fpath, paste0(name, "/names"))
        }

        for (i in seq_along(x)) {
            newname <- paste0(name, "/data/", i - 1L)
            tryCatch({
                .transform_list(x[[i]], dir=dir, path=path, fpath=fpath, name=newname, env=env)
            }, error=function(e) {
                s <- if (is.null(nn)) i else paste0("'", nn[i], "'")
                stop("failed to stage list element ", s, "\n  - ", e$message)
            })
        }

        return(NULL)
    }

    if (is.null(x)) {
        .label_group(fpath, name, uzuki_object="nothing")
        return(NULL)
    }

    if (is.null(dim(x))) {
        if (is.factor(x)) {
            .label_group(fpath, name, 
                uzuki_object="vector",
                uzuki_type=if (is.ordered(x)) "ordered" else "factor"
            )

            h5write(as.integer(x) - 1L, fpath, paste0(name, "/data"))
            h5write(levels(x), fpath, paste0(name, "/levels"))
            .add_names(x, fpath, name)
            return(NULL)

        } else if (is(x, "Date") || is.character(x)) {
            .label_group(fpath, name, 
                uzuki_object="vector",
                uzuki_type=if (is(x, "Date")) "date" else "string"
            )

            y <- as.character(x)
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
            .add_names(x, fpath, name)
            return(NULL)

        } else if (is.atomic(x)) {
            .label_group(fpath, name, 
                uzuki_object="vector",
                uzuki_type=.remap_type(x)
            )

            if (is.logical(x) && anyNA(x)) {
                # Force the use of a regular integer for missing values.
                x <- as.integer(x)
            }

            h5write(x, fpath, paste0(name, "/data"))
            .add_names(x, fpath, name)

            return(NULL)
       }
    }

    # External object fallback.
    .label_group(fpath, name, uzuki_object="external")
    n <- length(env$collected)
    write_integer_scalar(fpath, name, "index", n)

    n <- n + 1L
    if (is.data.frame(x)) {
        x <- DataFrame(x, check.names=FALSE)
    }
    tryCatch({
        meta <- .stageObject(x, dir, paste0(path, paste0("/child-", n)), child=TRUE)
        env$collected[[n]] <- list(resource=.writeMetadata(meta, dir=dir))
    }, error=function(e) {
        stop("failed to stage '", class(x)[1], "' entry inside a list\n  - ", e$message)
    }) 
}

#' @importFrom rhdf5 H5Fopen H5Fclose H5Gopen H5Gclose h5writeAttribute H5Dopen H5Dclose
.label_group <- function(file, name, ...) {
    fhandle <- H5Fopen(file)
    on.exit(H5Fclose(fhandle), add=TRUE)
    ghandle <- H5Gopen(fhandle, name)
    on.exit(H5Gclose(ghandle), add=TRUE)

    attrs <- list(...)
    for (a in names(attrs)) {
        h5writeAttribute(attrs[[a]], h5obj=ghandle, name=a, asScalar=TRUE)
    }
}

#' @importFrom rhdf5 h5write
.add_names <- function(x, fpath, name) {
    if (!is.null(names(x))) {
        h5write(names(x), fpath, paste0(name, "/names"))
    }
}

#' @export
#' @rdname stageBaseList
setMethod("stageObject", "List", function(x, dir, path, child=FALSE, fname="list") {
    stageObject(as.list(x), dir, path, child=child, fname=fname)
})

