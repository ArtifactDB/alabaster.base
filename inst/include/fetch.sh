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

harvester comservatory https://github.com/ArtifactDB/comservatory v2.0.2
