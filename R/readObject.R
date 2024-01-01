#' Read an object from disk
#'
#' Read an object from its on-disk representation.
#' This is done by dispatching to an appropriate loading function based on the type in the \code{OBJECT} file.
#'
#' @param path String containing a path to a directory, itself created with a \code{\link{saveObject}} method.
#' @param ... Further arguments to pass to individual methods.
#' @param metadata Named list containing metadata for the object - most importantly, the \code{type} field that controls dispatch to the correct loading function.
#' If \code{NULL}, this is automatically read by \code{\link{readObjectFile}(path)}.
#' @param type String specifying the name of type of the object.
#' @param fun A loading function that accepts \code{path}, \code{metadata} and \code{...} (in that order), and returns the associated object.
#' This may also be \code{NULL} to delete an existing entry in the registry.
#' @param existing Logical scalar indicating the action to take if a function has already been registered for \code{type} -
#' keep the old or new function, or throw an error.
#' 
#' @return 
#' For \code{readObject}, an object created from the on-disk representation in \code{path}.
#'
#' For \code{readObjectFunctionRegistry}, a named list of functions used to load each object type.
#'
#' For \code{registerReadObjectFunction}, the function is added to the registry.
#'
#' @section Comments for extension developers:
#' \code{readObject} uses an internal registry of functions to decide how an object should be loaded into memory.
#' Developers of alabaster extensions can add extra functions to this registry, usually in the \code{\link{.onLoad}} function of their packages.
#' Alternatively, extension developers can request the addition of their packages to default registry.
#'
#' If a loading function makes use of additional arguments, it should be scoped by the name of the class for each method, e.g., \code{list.parallel}, \code{dataframe.include.nested}.
#' This avoids problems with conflicts in the interpretation of identically named arguments between different functions.
#' It is expected that arguments in \code{...} are forwarded to internal \code{\link{altReadObject}} calls.
#'
#' When writing alabaster extensions, developers may need to load child objects inside the loading functions for their classes. 
#' In such cases, developers should use \code{\link{altReadObject}} rather than calling \code{readObject} directly.
#' This ensures that any application-level overrides of the loading functions are respected. 
#' Once in memory, the child objects can then be assembled into more complex objects by the developer's loading function.
#'
#' Developers can manually control \code{\link{readObject}} dispatch by suppling a \code{metadata} list where \code{metadata$type} is set to the desired object type.
#' This pattern is commonly used inside the loading function for a subclass, to construct the base class first before adding the subclass-specific components.
#' In practice, base construction should be done using \code{\link{altReadObject}} so as to respect application-specific overrides.
#'
#' @section Comments for application developers:
#' Application developers can override the behavior of \code{readObject} by specifying a custom function in \code{\link{altReadObject}}.
#' This can be used to point to a different registry of reading functions, to perform pre- or post-reading actions, etc.
#' If customization is type-specific, the custom \code{altReadObject} function can read the type from the \code{OBJECT} file to determine the most appropriate course of action;
#' this type information may then be passed to the \code{type} argument of \code{readObject} to avoid a redundant read from the same file.
#'
#' @author Aaron Lun
#' @examples
#' library(S4Vectors)
#' df <- DataFrame(A=1:10, B=LETTERS[1:10])
#'
#' tmp <- tempfile()
#' saveObject(df, tmp)
#' readObject(tmp)
#' 
#' @export
#' @aliases loadObject schemaLocations customloadObjectHelper .loadObjectInternal 
readObject <- function(path, metadata=NULL, ...) {
    if (is.null(metadata)) {
        metadata <- readObjectFile(path)
    }
    type <- metadata$type

    meth <- read.registry$registry[[type]]
    if (is.null(meth)) {
        stop("cannot read unknown object type '", type, "'")
    } else if (is.character(meth)) {
        meth <- eval(parse(text=meth))
        read.registry$registry[[type]] <- meth
    }
    meth(path, metadata=metadata, ...)
}

read.registry <- new.env()
read.registry$registry <- list(
    atomic_vector="alabaster.base::readAtomicVector",
    string_factor="alabaster.base::readBaseFactor",
    simple_list="alabaster.base::readBaseList",
    data_frame="alabaster.base::readDataFrame",
    data_frame_factor="alabaster.base::readDataFrameFactor",
    dense_array="alabaster.matrix::readArray",
    compressed_sparse_matrix="alabaster.matrix::readSparseMatrix",
    summarized_experiment="alabaster.se::readSummarizedExperiment",
    ranged_summarized_experiment="alabaster.se::readRangedSummarizedExperiment",
    single_cell_experiment="alabaster.sce::readSingleCellExperiment",
    genomic_ranges="alabaster.ranges::readGRanges",
    genomic_ranges_list="alabaster.ranges::readGRangesList",
    data_frame_list="alabaster.ranges::readDataFrameList",
    atomic_vector_list="alabaster.ranges::readAtomicVectorList",
    sequence_information="alabaster.ranges::readSeqinfo",
    multi_sample_dataset="alabaster.mae::readMultiAssayExperiment",
    sequence_string_set="alabaster.string::readXStringSet",
    spatial_experiment="alabaster.spatial::readSpatialExperiment",
    bam_file="alabaster.files::readBamFileReference",
    bcf_file="alabaster.files::readBcfFileReference",
    bigbed_file="alabaster.files::readBigBedFileReference",
    bigwig_file="alabaster.files::readBigWigFileReference",
    bed_file="alabaster.files::readBedFileReference",
    gff_file="alabaster.files::readGffFileReference",
    gmt_file="alabaster.files::readGmtFileReference",
    fasta_file="alabaster.files::readFastaFileReference",
    fastq_file="alabaster.files::readFastqFileReference",
    bumpy_data_frame_array="alabaster.bumpy::readBumpyDataFrameMatrix"
)

#' @export
#' @rdname readObject
readObjectFunctionRegistry <- function() {
    read.registry$registry
}

#' @export
#' @rdname readObject
registerReadObjectFunction <- function(type, fun, existing=c("old", "new", "error")) {
    if (!is.null(fun)) {
        if (!is.null(read.registry$registry[[type]])) {
            existing <- match.arg(existing)
            if (existing == "old") {
                return(invisible(NULL))
            } else if (existing == "error") {
                stop("function has already been registered for object type '", type, "'")
            }
        }
    }
    read.registry$registry[[type]] <- fun
    invisible(NULL)
}

#######################################
########### OLD STUFF HERE ############
#######################################

#' @export
loadObject <- function(info, project, ...) {
    customloadObjectHelper(info, 
        project, 
        ...,
        .locations=.default_schema_locations(),
        .memory=restore.memory
    )
}

restore.memory <- new.env()
restore.memory$cache <- list()

#' @export
#' @importFrom jsonlite fromJSON
customloadObjectHelper <- function(info, project, ..., .locations, .memory, .fallback=NULL) {
    schema <- info[["$schema"]]

    if (is.null(FUN <- .memory$cache[[schema]])) {
        schema.path <- .hunt_for_schemas(schema, .locations, .fallback=.fallback)

        schema.data <- fromJSON(schema.path, simplifyVector=TRUE, simplifyMatrix=FALSE, simplifyDataFrame=FALSE)
        restore <- schema.data[["_attributes"]][["restore"]][["R"]]
        if (is.null(restore)) {
            stop("could not find an R context to restore '", schema, "'")
        }

        FUN <- eval(parse(text=restore))
        .memory$cache[[schema]] <- FUN
    }

    FUN(info, project, ...)
}

#' @export
schemaLocations <- function() {
    .Defunct()
}

#' @import alabaster.schemas
.default_schema_locations <- function() c(getOption("alabaster.schema.locations"), "alabaster.schemas")

.hunt_for_schemas <- function(schema, .locations, .fallback=NULL) {
    schema.path <- "" 
    for (pkg in .locations) {
        schema.path <- system.file("schemas", schema, package=pkg)
        if (schema.path != "") {
            break
        }
    }

    if (schema.path == "") {
        if (is.null(.fallback)) {
            stop("couldn't find the '", schema, "' schema")
        }
        schema.path <- .fallback(schema)
    }

    schema.path
}

# Soft-deprecated backwards compatibility fixes.

#' @export
.loadObjectInternal <- function(...) customloadObjectHelper(...)
