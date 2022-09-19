#!/bin/bash

if [ -e comservatory ]
then
    (cd comservatory && git pull)
else
    git clone https://github.com/LTLA/comservatory comservatory
fi

cd comservatory
git checkout 216a2b266d06e38c6f87523109d821ecb9148771
if [ ! -e build ]
then
    cmake -S . -B build
fi
cd -

rm -rf include/comservatory
cp -r comservatory/include/ include

rm -rf include/byteme
cp -r comservatory/build/_deps/byteme-src/include/byteme include
