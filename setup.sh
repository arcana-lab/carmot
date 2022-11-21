#!/bin/bash

# Get repo dir
REPO_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )" ;

# Setup environment variables
source ${REPO_PATH}/setup_env.sh ;

# Check whether noelle repo was cloned
pathToRepo="${REPO_PATH}/tools/noelle" ;
if ! test -d ${pathToRepo} ; then
  mkdir -p ${REPO_PATH}/tools ;
  cd ${REPO_PATH}/tools ;
  git clone https://github.com/arcana-lab/noelle.git noelle ;
  cd noelle ;
  git checkout 44d92092000239c11e80c81727ef5a907648d93b ;

  # Hack: add noelle-norm-nomem2reg
  cp ${REPO_PATH}/noelle_patch/noelle-norm-nomem2reg ${pathToRepo}/src/core/scripts/ ;
  cp ${REPO_PATH}/noelle_patch/installNOELLE.sh ${pathToRepo}/src/core/scripts/ ;
fi

# Build noelle repo
cd ${pathToRepo} ;
make clean ;
make uninstall ;
make ;

# Compile MEMORYTOOL llvm pass
cd ${REPO_PATH} ;
./compile.sh ;

# Build runtime
cd ${REPO_PATH}/include ;
./compile.sh ;

# Build tag_functions_with_opt_none pass
cd ${REPO_PATH}/utils/tag_functions_with_opt_none ;
./compile.sh ;

# Build remove_optnone pass
cd ${REPO_PATH}/utils/remove_optnone ;
./compile.sh ;

# Build noelle_profiler pass
#cd ${REPO_PATH}/utils/noelle_profiler ;
#./compile.sh ;

# Build prettyPrint
cd ${REPO_PATH}/utils/prettyPrint ;
make ;

# Build PIN tool
cd ${REPO_PATH}/utils/MemTrace ;
make ;

# Go back to repo root
cd ${REPO_PATH} ;
