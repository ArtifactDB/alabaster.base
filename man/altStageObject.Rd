% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/altStageObject.R
\name{.stageObject}
\alias{.stageObject}
\alias{.altStageObject}
\title{Alter the staging generic}
\usage{
.stageObject(...)

.altStageObject(generic)
}
\arguments{
\item{...}{Further arguments to pass to \code{\link{stageObject}} or an equivalent generic.}

\item{generic}{Generic function that can serve as a drop-in replacement for \code{\link{stageObject}}.}
}
\value{
For \code{.stageObject}, a named list similar to the return value of \code{\link{stageObject}}. 

For \code{.altStageObject}, the alternative generic (if any) is returned if \code{generic} is missing.
If \code{generic} is provided, it is used to define the alternative, and the previous alternative is returned.
}
\description{
Allow Artificer applications to divert to a different staging generic from \code{\link{stageObject}}.
}
\details{
\code{.stageObject} is just a wrapper around \code{\link{stageObject}} that responds to any setting of \code{.altStageObject}.
This allows applications to inject customizations into the staging process, e.g., to store more metadata to particular objects.
Developers of Artificer extensions should use \code{.stageObject} to stage child objects when writing their \code{stageObject} methods.

To motivate the use of \code{.stageObject}, consider the following scenario.
\enumerate{
\item We have created a staging method for class X, defined for the \code{\link{stageObject}} generic.
\item An Artificer application Y requires the addition of some custom metadata during the staging process for X.
It defines an alternative staging generic \code{stageObject2} that, upon encountering an instance of X, redirects to a application-specific method.
For example, the \code{stageObject2} method for X could call X's \code{stageObject} method, add the necessary metadata to the result, and then return the list.
\item When operating in the context of application Y, the \code{stageObject2} generic is used to set \code{.altStageObject}.
Any calls to \code{.stageObject} in Y's context will subsequently call \code{stageObject2}.
\item So, when writing a \code{stageObject} method for any objects that might include X as children, we use \code{\link{.stageObject}} instead of directly using \code{\link{stageObject}}. 
This ensures that, if a child instance of X is encountered \emph{and} we are operating in the context of application Y, 
we correctly call \code{stageObject2} and then ultimately the application-specific method.
}

For \emph{application} developers: the alternative \code{stageObject2} method for X should \emph{not} call \code{.stageObject} on the same instance of X.
Doing so will introduce an infinite recursion where \code{.stageObject} calls the alternative generic that then calls \code{.stageObject}, etc.
Rather, developers should call \code{stageObject} directly in such cases.
For child objects, no infinite recursion will occur and either \code{stageObject2} or \code{.stageObject} can be used.
}
\examples{
old <- .altStageObject()

# Creating a new generic for demonstration purposes:
setGeneric("superStageObject", function(x, dir, path, child=FALSE, ...)
    standardGeneric("superStageObject"))

setMethod("superStageObject", "ANY", function(x, dir, path, child=FALSE, ...) {
    print("Falling back to the base method!")
    stageObject(x, dir, path, child=child, ...)
})

.altStageObject(superStageObject)

# Staging an example DataFrame. This should print our message.
library(S4Vectors)
df <- DataFrame(A=1:10, B=LETTERS[1:10])
tmp <- tempfile()
dir.create(tmp)
out <- .stageObject(df, tmp, path="coldata")

# Restoring the old loader:
.altStageObject(old)

}
\author{
Aaron Lun
}
