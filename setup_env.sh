#!/bin/bash

# Get repo dir
export REPO_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )" ;

# Enable correct version of llvm
#source /project/extra/llvm/9.0.0/enable ; 

# Enable boost
#source /project/extra/boost/1.72.0/enable ;

# Deactivate virtualenv, just in case, because I'm lazy
#deactivate ;

# Initialize python virtual environment
#source ${REPO_PATH}/virtualEnv.sh ;

# Add the path to the runtime shared library to LD_LIBRARY_PATH
export LD_LIBRARY_PATH=${REPO_PATH}/include:${LD_LIBRARY_PATH} ;

# Add memorytool to PATH
export PATH=${PATH}:${REPO_PATH}/utils ;

# Add prettyPrint to PATH
export PATH=${PATH}:${REPO_PATH}/utils/prettyPrint ;

# Add NOELLE to PATH
export PATH=${PATH}:${REPO_PATH}/tools/noelle/install/bin ;

# Add pin
source ${REPO_PATH}/extra/pin/3.13/enable ;
export PIN_ROOT="${REPO_PATH}/extra/pin/3.13/download" ;

# Go back to repo root
cd ${REPO_PATH} ;
