#!/bin/bash

if [ -e uzuki2 ]
then
    (cd uzuki2 && git pull)
else
    git clone https://github.com/LTLA/uzuki2
fi

cd uzuki2
git checkout 81285baa92520a9bc6758498ca5f3476867e4331
if [ ! -e build ]
then
    cmake -S . -B build
fi
cd -

rm -rf include/uzuki2
cp -r uzuki2/include/uzuki2 include/

rm -rf include/millijson
cp -r uzuki2/build/_deps/millijson-src/include/millijson include/
