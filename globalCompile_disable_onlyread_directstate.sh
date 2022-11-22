#!/bin/bash -e

# Get repo dir
REPO_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )" ;

cd ${REPO_PATH}/include ;
./compile.sh

cd ${REPO_PATH} ;
./compile_disable_onlyread_directstate.sh ;

cd ${REPO_PATH}/utils/remove_optnone ;
./compile.sh ;

cd ${REPO_PATH}/utils/tag_functions_with_opt_none ;
./compile.sh ;

#cd ${REPO_PATH}/utils/noelle_profiler ;
#./compile.sh ;

cd ${REPO_PATH}/utils/clone_function_roi ;
./compile_disable.sh ;

cd ${REPO_PATH}/utils/promote_cloned_function_allocas ;
./compile_disable.sh ;

cd ${REPO_PATH}/utils/check_for_induction_variables ;
./compile_disable.sh ;
