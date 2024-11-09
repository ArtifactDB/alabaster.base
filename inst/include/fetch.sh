#!/bin/bash

set -e
set -u

# Fetches all of the header files. We vendor it inside the package so that
# downstream packages can simply use LinkingTo to get access to them.

harvester() {
    local name=$1
    local url=$2
    local version=$3

    local tmpname=source-${name}
    if [ ! -e $tmpname ]
    then 
        git clone $url $tmpname
    else 
        cd $tmpname
        git checkout master
        git pull
        cd -
    fi

    cd $tmpname
    git checkout $version
    rm -rf ../$name
    cp -r include/$name ../$name
    cd -
}

harvester millijson https://github.com/ArtifactDB/millijson v1.0.1 
harvester byteme https://github.com/LTLA/byteme v1.1.0
harvester comservatory https://github.com/ArtifactDB/comservatory v2.0.1
harvester uzuki2 https://github.com/ArtifactDB/uzuki2 v1.4.0
harvester ritsuko https://github.com/ArtifactDB/ritsuko v0.5.2
harvester takane https://github.com/ArtifactDB/takane v0.7.1
harvester chihaya https://github.com/ArtifactDB/chihaya v1.1.0
