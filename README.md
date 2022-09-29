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

Check out the [user's guide](https://artifactdb.github.io/alabaster.base/articles/userguide.html) for more details.
The same process can be applied to `GRanges`, `SummarizedExperiment`, `SingleCellExperiment` objects, etc., provided the appropriate **alabaster** package is installed.

## Extensions and applications

Developers can _extend_ this framework to support more R/Bioconductor classes by creating their own **alabaster** package.
Check out the [extension guide](https://artifactdb.github.io/alabaster.base/articles/extensions.html) for more details.

Developers can also _customize_ this framework for specific applications, most typically to save bespoke metadata in the JSON file.
The JSON file can then be indexed by systems like MongoDB and Elasticsearch to provide search capabilities.
Check out the [applications guide](https://artifactdb.github.io/alabaster.base/articles/applications.html) for more details.

## Links

The [**BiocObjectSchemas**](https://github.com/ArtifactDB/BiocObjectSchemas) repository contains schema definitions for many Bioconductor objects.

For use in an R installation, all schemas are packaged in the [**alabaster.schemas**](https://github.com/ArtifactDB/alabaster.schemas) R package.

A [Docker image](https://github.com/ArtifactDB/alabaster-docker/pkgs/container/alabaster-docker%2Fbuilder) is available, containing several pre-installed **alabaster** packages.
