\name{alabaster.base News}
\title{alabaster.base News}
\encoding{UTF-8}

\section{Version 1.8.0}{\itemize{
\item Added \code{createDedupSession()}, \code{addObjectToDedupSession()} and \code{checkObjectInDedupSession()} utilities,
that allow \code{saveObject()} method developers to check for duplicated objects and avoid re-saving them.
Most of this functionality was migrated and generalized from the \pkg{alabaster.matrix} package. 

\item Added the \code{cloneDirectory()} function to easily clone the contents of an existing directory, either by copying or linking to the files.
This should be used for creating lightweight on-disk representations of duplicated objects.

\item Added the \code{absolutizePath()} function to obtain an absolute file path.
This is occasionally necessary to ensure that references to deduplicated resources are robust to changes to the working directory.

\item Coerce \code{numeric_version} instances to strings or character vectors before saving them in \code{saveObject()}.
This includ such objects encountered inside lists or DataFrames.

\item Added \code{getSaveEnvironment()} and related functions to keep track of the R environment used to save each object.
This facilitates debugging and enables corrective actions to be taken by loading functions when encountering files created by buggy versions of any package in the \pkg{alabaster} stack.
}}

\section{Version 1.6.0}{\itemize{
\item Distinguish between scalars and length-1 vectors when saving/loading lists.
This effectively unboxes all length-1 vectors in a list, by default; this is probably the more reasonable expectation for other languages that have a concept of scalars.
Users can override this by calling \code{I()} on elements that they want to keep as length-1 vectors, in the same manner as \pkg{jsonlite}.

\item Streamlined the definition of the Rfc3339 class so that it behaves better with \code{I()}.

\item Normalize paths to resolve \code{~} prior to calling C++ code.

\item Open HDF5 files in read-only mode to avoid permission-related problems for \code{readObject()}-dispatched functions.

\item Store numbers at maximum precision when saving lists in the JSON format via \code{saveObject()}.

\item Added \code{registerValidateObjectSatisfiesInterface()} and \code{registerValidateObjectDerivedFrom()},
to allow developers to declare that custom subclasses satisfy an interface or have an inheritance relationship, respectively. 

\item Updated \code{validateDirectory()} so that it works with a directory of objects saved via \code{saveObject()}.
Objects saved under the old regime (i.e., \code{stageObject()}) are auto-detected but can also be explicitly validated by setting \code{legacy=FALSE}.

\item Added a data.frame method for \code{saveObject()}, to avoid fallback to the list method.
}}

\section{Version 1.4.0}{\itemize{
\item Added the \code{saveObject} and \code{readObject} functions,
which comply with the newly formalized \pkg{takane} specifications for each object's on-disk representation.
This enables greater consistency across language frameworks (e.g., Python) by using a common library for enforcing the specification. 
It also eliminates the need for a separate metadata file, simplifying the user experience and the expectations on extension developers.

\item Added a dedicated \code{validateObject} function, which uses the \pkg{takane} libary to validate each object's files.
This allows users to easily check if a set of files is compliant with the specification, e.g., when modifying parts of the object outside of \code{saveObject}.

\item Soft-deprecation of the \code{stageObject} and \code{loadObject} functions.
These use legacy representations that do not comply with the tightened \pkg{takane} specifications. 
}}
