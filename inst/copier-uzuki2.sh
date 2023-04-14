#!/bin/bash

if [ -e uzuki2 ]
then
    (cd uzuki2 && git pull)
else
    git clone https://github.com/LTLA/uzuki2
fi

cd uzuki2
git checkout fceb37ef0ecf48bc6d29e9c6c4cb5c6d18636fb8
if [ ! -e build ]
then
    cmake -S . -B build
fi
cd -

rm -rf include/uzuki2
cp -r uzuki2/include/uzuki2 include/

rm -rf include/millijson
cp -r uzuki2/build/_deps/millijson-src/include/millijson include/
