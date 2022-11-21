#!/bin/bash

# Get repo dir
REPO_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )" ;

# Clean tests
pushd . ;
cd ${REPO_PATH}/tests/regression ;
make clean;
popd ;

# Clean runtime
pushd . ;
cd ${REPO_PATH}/include ;
make clean ;
popd ;

# Clean PIN tool
pushd . ;
cd ${REPO_PATH}/utils/MemTrace ;
make clean ;
popd ;

# Clean prettyPrint
pushd . ;
cd ${REPO_PATH}/utils/prettyPrint ;
make clean ;
popd ;

# Clean tag_functions_with_opt_none
pushd . ;
cd ${REPO_PATH}/utils/tag_functions_with_opt_none ;
./clean.sh ;
popd ;

# Clean remove_optnone
pushd . ;
cd ${REPO_PATH}/utils/remove_optnone ;
./clean.sh ;
popd ;

# Clean noelle_profiler
pushd . ;
cd ${REPO_PATH}/utils/noelle_profiler ;
./clean.sh ;
popd ;

# Clean clone_function_roi
pushd . ;
cd ${REPO_PATH}/utils/clone_function_roi ;
./clean.sh ;
popd ;

# Clean promote_cloned_function_allocas
pushd . ;
cd ${REPO_PATH}/utils/promote_cloned_function_allocas ;
./clean.sh ;
popd ;

# Clean check_for_induction_variables
pushd . ;
cd ${REPO_PATH}/utils/check_for_induction_variables ;
./clean.sh ;
popd ;

# Remove unnecessary dirs
rm -rf ${REPO_PATH}/virtualEnv ;
rm -rf ${REPO_PATH}/tools ;
rm -rf ${REPO_PATH}/build ;
rm -rf ${REPO_PATH}/inst ;
