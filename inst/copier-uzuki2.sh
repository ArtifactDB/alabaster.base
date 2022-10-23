#!/bin/bash

if [ -e uzuki2 ]
then
    (cd uzuki2 && git pull)
else
    git clone https://github.com/LTLA/uzuki2
fi

cd uzuki2
git checkout a06e7e835904c57cea111d47c097d5fdcb329dc9
if [ ! -e build ]
then
    cmake -S . -B build
fi
cd -

rm -rf include/uzuki2
cp -r uzuki2/include/uzuki2 include/

rm -rf include/millijson
cp -r uzuki2/build/_deps/millijson-src/include/millijson include/
