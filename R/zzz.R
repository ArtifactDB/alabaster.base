.onLoad <- function(libname, pkgname) {
    register_any_duplicated(set=TRUE)
}

.onUnload <- function(libname, pkgname) {
    register_any_duplicated(set=FALSE)
}
