#!/bin/bash

if [ -e uzuki ]
then
    (cd uzuki && git pull)
else
    git clone https://github.com/LTLA/uzuki 
fi

cd uzuki 
git checkout d1f2ca4f4da004a65ab0815d07703883753470ab
if [ ! -e build ]
then
    cmake -S . -B build
fi
cd -

rm -rf include/uzuki
cp -r uzuki/include/ include

rm -rf include/nlohmann
mkdir -p include/nlohmann
cp -r uzuki/build/_deps/json-src/single_include/nlohmann/json.hpp include/nlohmann
