#!/bin/bash -e

# Set the CLANG compilers to use if unset
if test -z "$CC" ; then
  export CC=clang
  export CXX=clang++
fi

export REPO_PATH=`pwd` ;
rm -rf build/ ; 
mkdir build ; 
cd build ; 
mkdir -p ${REPO_PATH}/inst ;
cmake3 -DCMAKE_INSTALL_PREFIX="${REPO_PATH}/inst" -DCMAKE_BUILD_TYPE=Debug -DMEMORYTOOL_DISABLE_LOCALS_OPT=ON ../ ; 
make -j ;
make install ;
cd ../ 
