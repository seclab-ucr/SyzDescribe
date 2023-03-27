#!/bin/bash

PATH_cmake=build

rm -rf $PATH_cmake
mkdir $PATH_cmake
cd $PATH_cmake || exit
echo "[*] Trying to cmake"
cmake -G "Unix Makefiles" ..
echo "[*] Trying to make"
make -j16
