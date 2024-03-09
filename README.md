# Save Bioconductor objects to file

## Overview

The **alabaster** framework implements methods to save a variety of R/Bioconductor objects to on-disk representations.
This is a more robust and portable alternative to the typical approach of saving objects in RDS files.

- By separating the on-disk representation from the in-memory object structure, we can more easily adapt to changes in S4 class definitions.
  This improves robustness to R environment updates, especially when `updateObject()` is not correctly configured.
- By using standard file formats like HDF5 and JSON, we ensure that Bioconductor objects can be easily read from other languages like Python and Javascript.
  This improves interoperability between application ecosystems.
- By breaking up complex Bioconductor objects into their components, we enable modular reads and writes to the backing store.
  We can easily read or update part of an object without having to consider the other parts.

The **alabaster.base** package defines the base generics to read and write the file structures along with the associated metadata.
Implementations of these methods for various Bioconductor classes can be found in the other **alabaster** packages like 
[**alabaster.se**](https://github.com/ArtifactDB/alabaster.se) and [**alabaster.bumpy**](https://github.com/ArtifactDB/alabaster.bumpy).

## Quick start

First, we'll install the **alabaster.base** package.
This package is available from [Bioconductor](https://bioconductor.org/packages/alabaster.base),
so we can use the standard Bioconductor installation process:

```r
# install.packages("BiocManager")
BiocManager::install("alabaster.base")
```

The simplest example involves saving a `DataFrame` inside a staging directory.
Let's mock up an object:

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
library(alabaster.base)
saveObject(df, tmp)
```

We can copy the directory to another location, over a network, etc., and then easily load it back into a new R session:

```r
readObject(tmp)
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

## Supported classes

The saving/reading process can be applied to a range of data structures, provided the appropriate **alabaster** package is installed.

| Package | Object types | BioC-devel | BioC-release |
|-----|-----|----|----|
| [**alabaster.base**](https://github.com/ArtifactDB/alabaster.base) | `list`, `factor`, [`DataFrame`](https://bioconductor.org/packages/S4Vectors), `List` | [![](http://bioconductor.org/shields/build/devel/bioc/alabaster.base.svg)](http://bioconductor.org/checkResults/devel/bioc-LATEST/alabaster.base) | [![](http://bioconductor.org/shields/build/release/bioc/alabaster.base.svg)](http://bioconductor.org/checkResults/release/bioc-LATEST/alabaster.base) |
| [**alabaster.matrix**](https://github.com/ArtifactDB/alabaster.matrix) | `matrix`, [`Matrix`](https://cran.r-project.org/web/packages/Matrix/index.html) objects, [`DelayedArray`](https://bioconductor.org/packages/DelayedArray) | [![](http://bioconductor.org/shields/build/devel/bioc/alabaster.matrix.svg)](http://bioconductor.org/checkResults/devel/bioc-LATEST/alabaster.matrix) | [![](http://bioconductor.org/shields/build/release/bioc/alabaster.matrix.svg)](http://bioconductor.org/checkResults/release/bioc-LATEST/alabaster.matrix) |
| [**alabaster.ranges**](https://github.com/ArtifactDB/alabaster.ranges) | [`GRanges`](https://bioconductor.org/packages/GenomicRanges), `GRangesList` and related objects | [![](http://bioconductor.org/shields/build/devel/bioc/alabaster.ranges.svg)](http://bioconductor.org/checkResults/devel/bioc-LATEST/alabaster.ranges) | [![](http://bioconductor.org/shields/build/release/bioc/alabaster.ranges.svg)](http://bioconductor.org/checkResults/release/bioc-LATEST/alabaster.ranges) |
| [**alabaster.se**](https://github.com/ArtifactDB/alabaster.se) | [`SummarizedExperiment`](https://bioconductor.org/packages/SummarizedExperiment), `RangedSummarizedExperiment` | [![](http://bioconductor.org/shields/build/devel/bioc/alabaster.se.svg)](http://bioconductor.org/checkResults/devel/bioc-LATEST/alabaster.se) | [![](http://bioconductor.org/shields/build/release/bioc/alabaster.se.svg)](http://bioconductor.org/checkResults/release/bioc-LATEST/alabaster.se) |
| [**alabaster.sce**](https://github.com/ArtifactDB/alabaster.sce) | [`SingleCellExperiment`](https://bioconductor.org/packages/SingleCellExperiment) | [![](http://bioconductor.org/shields/build/devel/bioc/alabaster.sce.svg)](http://bioconductor.org/checkResults/devel/bioc-LATEST/alabaster.sce) | [![](http://bioconductor.org/shields/build/release/bioc/alabaster.sce.svg)](http://bioconductor.org/checkResults/release/bioc-LATEST/alabaster.sce) |
| [**alabaster.mae**](https://github.com/ArtifactDB/alabaster.mae) | [`MultiAssayExperiment`](https://bioconductor.org/packages/MultiAssayExperiment) | [![](http://bioconductor.org/shields/build/devel/bioc/alabaster.mae.svg)](http://bioconductor.org/checkResults/devel/bioc-LATEST/alabaster.mae) | [![](http://bioconductor.org/shields/build/release/bioc/alabaster.mae.svg)](http://bioconductor.org/checkResults/release/bioc-LATEST/alabaster.mae) |
| [**alabaster.string**](https://github.com/ArtifactDB/alabaster.string) | [`XStringSet`](https://bioconductor.org/packages/Biostrings) | [![](http://bioconductor.org/shields/build/devel/bioc/alabaster.string.svg)](http://bioconductor.org/checkResults/devel/bioc-LATEST/alabaster.string) | [![](http://bioconductor.org/shields/build/release/bioc/alabaster.string.svg)](http://bioconductor.org/checkResults/release/bioc-LATEST/alabaster.string) |
| [**alabaster.spatial**](https://github.com/ArtifactDB/alabaster.spatial) | [`SpatialExperiment`](https://bioconductor.org/packages/SpatialExperiment) | [![](http://bioconductor.org/shields/build/devel/bioc/alabaster.spatial.svg)](http://bioconductor.org/checkResults/devel/bioc-LATEST/alabaster.spatial) | [![](http://bioconductor.org/shields/build/release/bioc/alabaster.spatial.svg)](http://bioconductor.org/checkResults/release/bioc-LATEST/alabaster.spatial) |
| [**alabaster.bumpy**](https://github.com/ArtifactDB/alabaster.bumpy) | [`BumpyMatrix`](https://bioconductor.org/packages/BumpyMatrix) objects | [![](http://bioconductor.org/shields/build/devel/bioc/alabaster.bumpy.svg)](http://bioconductor.org/checkResults/devel/bioc-LATEST/alabaster.bumpy) | [![](http://bioconductor.org/shields/build/release/bioc/alabaster.bumpy.svg)](http://bioconductor.org/checkResults/release/bioc-LATEST/alabaster.bumpy) |
| [**alabaster.vcf**](https://github.com/ArtifactDB/alabaster.vcf) | [`VCF`](https://bioconductor.org/packages/Biostrings) objects | [![](http://bioconductor.org/shields/build/devel/bioc/alabaster.vcf.svg)](http://bioconductor.org/checkResults/devel/bioc-LATEST/alabaster.vcf) | [![](http://bioconductor.org/shields/build/release/bioc/alabaster.vcf.svg)](http://bioconductor.org/checkResults/release/bioc-LATEST/alabaster.vcf) |
| [**alabaster.files**](https://github.com/ArtifactDB/alabaster.files) | Common bioinformatics files, e.g., FASTQ, BAM | [![](http://bioconductor.org/shields/build/devel/bioc/alabaster.files.svg)](http://bioconductor.org/checkResults/devel/bioc-LATEST/alabaster.files) | [![](http://bioconductor.org/shields/build/release/bioc/alabaster.files.svg)](http://bioconductor.org/checkResults/release/bioc-LATEST/alabaster.files) |

All packages are available from Bioconductor and can be installed with the usual `BiocManager::install()` process.
Alternatively, to install all packages in one go, users can install the [**alabaster**](https://bioconductor.org/packages/alabaster) umbrella package.

## Extensions and applications

Developers can _extend_ this framework to support more R/Bioconductor classes by creating their own **alabaster** package.
Check out the [extension section](https://artifactdb.github.io/alabaster.base/articles/userguide.html#extending-to-new-classes) for more details.

Developers can also _customize_ this framework for specific applications, most typically to add bespoke metadata in the staging directory.
The metadata can then be indexed by database systems like SQLite and MongoDB to provide search capabilities.
Check out the [applications section](https://artifactdb.github.io/alabaster.base/articles/userguide.html#creating-applications) for more details.
