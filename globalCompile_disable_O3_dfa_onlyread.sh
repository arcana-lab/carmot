#!/bin/bash -e

# Get repo dir
REPO_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )" ;

cd ${REPO_PATH}/include ;
./compile.sh

cd ${REPO_PATH}/utils/tag_functions_with_opt_none ;
./compile_disable_O3.sh ;

cd ${REPO_PATH} ;
./compile_disable_dfa_onlyread.sh ;

cd ${REPO_PATH}/utils/noelle_profiler ;
./compile.sh ;
