# Make sure to put derived classes before base classes,
# otherwise the subclass look-up might not find the most appropriate method.
implementers <- list(
    SpatialExperiment = "alabaster.spatial", 

    SingleCellExperiment = "alabaster.sce",

    VCF = "alabaster.vcf",

    SummarizedExperiment = "alabaster.se",

    BumpyDataFrameMatrix = "alabaster.bumpy",

    GRanges = "alabaster.ranges",
    GRangesList = "alabaster.ranges",
    AtomicVectorList = "alabaster.ranges",
    DataFrameList = "alabaster.ranges",

    matrix = "alabaster.matrix",
    array = "alabaster.matrix",
    Matrix = "alabaster.matrix",
    DelayedArray = "alabaster.matrix",

    MultiAssayExperiment = "alabaster.mae",

    DNAStringSet = "alabaster.string"
)

package.lookup <- new.env()
package.lookup$found <- character(0)

package.exists <- function(pkg) {
    length(find.package(pkg, quiet=TRUE)) > 0
}

warn.package.exists <- function(pkg, cls) {
    warning("consider installing ", pkg, " for a more appropriate stageObject method for '", cls, "' objects")
}

.searchMethods <- function(x) {
    .searchForMethods(x, package.lookup, implementers)
}

#' @export
#' @import methods
.searchForMethods <- function(x, lookup, implements) {
    cls <- class(x)[1]

    if (! cls %in% lookup$found) {
        found <- NULL
        if (cls %in% names(implements)) {
            found <- implements[[cls]]
            if (!package.exists(found)) {
                warn.package.exists(found, cls)
                found <- NULL
            }
        } 

        if (is.null(found)) {
            for (y in names(implements)) {
                if (is(x, y)) {
                    if (package.exists(implements[[y]])) {
                        found <- implements[[y]]
                        break
                    } else {
                        warn.package.exists(implements[[y]], cls)
                    }
                }
            }
        }

        if (!is.null(found) && !isNamespaceLoaded(found)) {
            loadNamespace(found)
        }

        # Regardless of whether it was successful, we add the class,
        # so as to short-circuit any attempts in the future.
        lookup$found <- c(lookup$found, cls)
    }
}
