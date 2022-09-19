# Save Bioconductor objects to file

## Overview

The **alabaster** framework implements methods to save a variety of R/Bioconductor objects to on-disk representations.
This is a more robust and portable alternative to the typical approach of saving objects in RDS files.

- By separating the on-disk representation from the in-memory object structure, we can more easily adapt to changes in S4 class definitions.
  This improves robustness to R environment updates, especially when `updateObject()` is not correctly configured.
- By using standard file formats like HDF5 and CSV, we ensure that the contents of R/Bioconductor objects to be easily read from other languages like Python and Javascript.
  This improves interoperability between application ecosystems.
- By breaking up complex Bioconductor objects into their components, we enable modular reads and writes to the backing store.
  We can easily read or update part of an object without having to consider the other parts.

The **alabaster.base** package defines the base methods to read and write the file structures along with the associated metadata.
Implementations of these methods for various Bioconductor packages can be found in the other **alabaster** packages like **alabaster.se** and **alabaster.bumpy**.

## Quick start

The simplest example involves saving a `DataFrame` inside a staging directory.
Let's mock one up:

```r
library(S4Vectors)
df <- DataFrame(X=1:10, Y=letters[1:10])
## DataFrame with 10 rows and 2 columns
##            X           Y
##    <integer> <character>
## 1          1           a
## 2          2           b
## 3          3           c
## 4          4           d
## 5          5           e
## 6          6           f
## 7          7           g
## 8          8           h
## 9          9           i
## 10        10           j
```

Then we can save it to the staging directory:

```r
tmp <- tempfile()
dir.create(tmp)

library(alabaster.base)
meta <- stageObject(df, tmp, path="my_df")
```
  
This returns some metadata that can also be saved:

```r
.writeMetadata(meta, tmp)
cat(readLines(file.path(tmp, paste0(meta$path, ".json"))), sep="\n")
```

```json
{
  "$schema": "csv_data_frame/v2.json",
  "path": "my_df/simple.csv.gz",
  "is_child": false,
  "data_frame": {
    "columns": [
      {
        "name": "X",
        "type": "integer"
      },
      {
        "name": "Y",
        "type": "string",
        "values": ["a", "b", "c", "d", "e", "f", "g", "h", "i", "j"]
      }
    ],
    "dimensions": [10, 2]
  },
  "csv_data_frame": {
    "placeholder": true,
    "compression": "gzip"
  },
  "md5sum": "3be9b301364d2d69fd2f51b43b0086ea"
}
```

Loading it back into memory is straightforward:

```r
loadObject(meta, tmp)
## DataFrame with 10 rows and 2 columns
##            X           Y
##    <integer> <character>
## 1          1           a
## 2          2           b
## 3          3           c
## 4          4           d
## 5          5           e
## 6          6           f
## 7          7           g
## 8          8           h
## 9          9           i
## 10        10           j
```

The same process can be applied to `GRanges`, `SummarizedExperiment`, `SingleCellExperiment` objects, etc., provided the appropriate **alabaster** package is installed.
More complex objects will typically save themselves into multiple files, possibly by invoking the `stageObject()` methods of their child components;
for example, the staging/loading methods for a `SummarizedExperiment` would call the corresponding `DataFrame` methods for the `colData`.

## Extensions 

Developers can _extend_ this framework to support more R/Bioconductor classes by creating their own **alabaster** package.
To demonstrate, let's use a simple example of saving an ordinary R matrix into a HDF5 file, abbreviated from the **alabaster.matrix** code.

For saving, we need to define a new `stageObject()` method for our class of interest in our **alabaster.X** package.
This method will be called whenever an instance of that class is encountered, either directly or as a child of another object.
In the case below, we call `writeHDF5Array` to save the matrix to file; more sophisticated methods may choose to store attributes like the dimnames as well.

```r
setMethod("stageObject", "matrix", function(x, dir, path, child=FALSE) {
    dir.create(file.path(dir, path), showWarnings=FALSE)
    xpath <- file.path(path, "matrix.h5")
    ofile <- file.path(dir, xpath)
    HDF5Array::writeHDF5Array(x, filepath=ofile, name="data")
    list(
        `$schema`="my_dense_matrix/v1.json",
        path=xpath,
        is_child=child,
        array=list(dimensions=dim(x)),
        my_dense_matrix=list(format="HDF5")
    )
})
```

For validation, we create a [JSON schema](https://json-schema.org/) for the object's metadata.
This might look like:

```json
{
    "$schema": "http://json-schema.org/draft-07/schema",
    "$id": "my_dense_matrix/v1.json",
    "title": "My Dense Matrix",
    "type": "object"
    "description": "Demonstration of how to use a schema for my dense matrix type.",
    "properties": {
        "$schema": {
            "description": "The schema to use.",
            "type": "string"
        },
        "array": {
            "additionalProperties": false,
            "properties": {
                "dimensions": {
                    "description": "Dimensions of an n-dimensional object.",
                    "items": {
                        "type": "integer"
                    },
                    "minItems": 1,
                    "type": "array"
                }
            },
            "type": "object"
        },
        "my_dense_matrix": {
            "additionalProperties": false,
            "properties": {
                "format": {
                    "type": "string",
                    "enum": [ "HDF5", "TileDB", "CSV" ]
                }
            }
        },
        "is_child": {
            "default": false,
            "description": "Is this a child document?",
            "type": "boolean"
        },
        "md5sum": {
            "description": "MD5 checksum for the file.",
            "type": "string"
        },
        "path": {
            "description": "Path to the file in the project directory.",
            "type": "string"
        }
    },
    "required": [
        "$schema",
        "array",
        "my_dense_matrix",
        "md5sum",
        "path"
    ],
    "_attributes": {
        "format": "application/x-hdf5",
        "restore": {
            "R": "alabaster.X::loadMyDenseMatrix"
        }
    }
}
```

Finally, for loading, we need to define a new `load*` function for the new class that recreates the R object from the staged files.
In this case, it's fairly simple:

```r
loadMyDenseMatrix <- function(info, project) {
    path <- acquireFile(project, info$path)
    as.matrix(HDF5Array(path))
}
```

When `loadObject()` encounters an instance of a `my_dense_matrix/v1.json`, it will look at the schema's `_attributes.restore.R` field to figure out how to load it into R.
This will eventually lead us to `loadMyDenseMatrix()`, allowing developers to invoke their own custom loading methods.
Usually, `loadObject()` will look in **alabaster.schemas** to find the specified `$schema`, but this can be diversified to more locations via the `alabaster.schema.locations` global option.

## Customizations

Developers can also _customize_ this framework for specific applications, most typically to save more metadata in the JSON file.
The JSON file can then be indexed by systems like MongoDB and Elasticsearch to provide search capabilities.
A simple example would involve adding some author information during the staging process:

```r
setGeneric("stageObjectWithAuthor", function(x, dir, path, is.child=FALSE) standardGeneric("stageObjectWithAuthor"))

setMethod("stageObjectWithAuthor", "ANY", function(x, dir, path, is.child=FALSE) {
    stageObject(x, dir, path, is.child=is.child)
})

setMethod("stageObjectWithAuthor", "Annotated", function(x, dir, path, is.child=FALSE) {
    # Shift author information into the JSON metadata.
    author <- metadata(x)$author
    metadata(x)$author <- NULL
    if (!is.child) {
        if (!is.character(author) || length(author) == 0) {
            stop("non-child instance of ", class(x)[1], " should have an author")
        }
    }

    meta <- stageObject(x, dir, path, is.child=is.child) # re-use alabaster's methods
    meta$author <- I(author)
    meta
})

loadObjectWithAuthor <- function(info, project, ...) {
    x <- loadObject(info, project)
    if (is(x, "Annotated")) {
        metadata(x) <- info$author
    }
    x
}
```

Applications can then override the internal `stageObject()` and `loadObject()` calls in **alabaster.base**.
This means that any internal calls to `stageObject()` (e.g., to stage child components of a complex Bioconductor object) will instead call `stageObjectWithAuthor`, and similarly for loading.

```r
.altStageObject(stageObjectWithAuthor)
.altLoadObject(loadObjectWithAuthor)
```

Applications can even override how files and metadata are discovered by defining new methods for `acquireFile()` and `acquireMetadata()`.
This is typically used to instruct **alabaster.base** to look for files/metadata from remote sources instead of the local filesystem.

## Links

Link to ArtifactDB API.
