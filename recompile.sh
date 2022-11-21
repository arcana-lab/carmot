#!/bin/bash -e

# Set the CLANG compilers to use if unset
if test -z "$CC" ; then
  export CC=clang
  export CXX=clang++
fi

export REPO_PATH=`pwd` ;
cd build ;
make -j ;
make install ;
cd ../ 
