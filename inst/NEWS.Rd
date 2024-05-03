\name{scRNAseqnews}
\title{scRNAseq News}
\encoding{UTF-8}

\section{Version 1.6.0}{\itemize{
\item Distinguish between scalars and length-1 vectors when saving/loading lists.
This effectively unboxes all length-1 vectors in a list, by default; this is probably the more reasonable expectation for other languages that have a concept of scalars.
Users can override this by calling \code{I()} on elements that they want to keep as length-1 vectors, in the same manner as \pkg{jsonlite}.

\item Streamlined the definition of the Rfc3339 class so that it behaves better with \code{I()}.
}}

\section{Version 1.4.1}{\itemize{
\item Store numbers at maximum precision when saving lists in the JSON format via \code{saveObject}.
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
