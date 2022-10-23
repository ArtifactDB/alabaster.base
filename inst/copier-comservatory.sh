#!/bin/bash

if [ -e comservatory ]
then
    (cd comservatory && git pull)
else
    git clone https://github.com/LTLA/comservatory comservatory
fi

cd comservatory
git checkout 11c6f896127efabebc5875025f992800f02e1b10
if [ ! -e build ]
then
    cmake -S . -B build
fi
cd -

rm -rf include/comservatory
cp -r comservatory/include/comservatory include/

rm -rf include/byteme
cp -r comservatory/build/_deps/byteme-src/include/byteme include/
