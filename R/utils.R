.sanitize_date <- function(x) {
    format(x, "%Y-%m-%d")
}

.is_datetime <- function(x) {
    is(x, "POSIXct") || is(x, "POSIXlt") || is.Rfc3339(x)
}

.sanitize_datetime <- function(x) {
    sub("([0-9]{2})$", ":\\1", strftime(x, "%Y-%m-%dT%H:%M:%S%z"))
}

.remap_atomic_type <- function(x) {
    y <- typeof(x)

    # Forcibly coercing the types, just to make sure that
    # we don't get tricked by classes that might do something
    # different inside write.csv or whatever.
    switch(y,
        integer=list(type="integer", values=as.integer(x)),
        double=list(type="number", values=as.double(x)),
        numeric=list(type="number", values=as.double(x)),
        character=list(type="string", values=as.character(x)),
        logical=list(type="boolean", values=as.logical(x)),
        stop("type '", y, "' is not supported")
    )
}

.atomics <- c(integer="integer", number="double", string="character", boolean="logical")

.is_atomic <- function(type) {
    type %in% names(.atomics)
}

.cast_atomic <- function(x, type) {
    as(x, as.character(.atomics[type]))
}
