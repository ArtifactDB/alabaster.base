#!/bin/bash

if [ -e uzuki2 ]
then
    (cd uzuki2 && git pull)
else
    git clone https://github.com/LTLA/uzuki2
fi

rm -rf include/uzuki2
cp -r uzuki2/include/uzuki2 include/
