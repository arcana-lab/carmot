#!/bin/bash

# Get repo dir
REPO_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/.." ;

# Check whether runtime repo was cloned
pathToRuntime="${REPO_PATH}/tools/texas" ;
if ! test -d ${pathToRuntime} ; then
  mkdir -p ${REPO_PATH}/tools ;
  cd ${REPO_PATH}/tools ;
  
  #git clone /project/noelle/repositories/texas ;
  #cd ${REPO_PATH}/tools/texas ;
  #git checkout texasCarmotAllocReuse ;

  cp -r ${REPO_PATH}/extra/texas . ;

  cd - ;
fi

# Check if there is a link to caratRT in include
if ! test -d ${REPO_PATH}/include/runtime ; then
  cd ${REPO_PATH}/include ;
  ln -s ../tools/texas/src/runtime runtime ;
fi

#cd ${pathToRuntime} ;
#isDetachedHEAD=`git rev-parse --abbrev-ref --symbolic-full-name HEAD` ;
# Pull the repo, if HEAD is not detached
#if [ ${isDetachedHEAD} != "HEAD" ] ; then
#  git pull ;
#fi

#Add oneTBB and other texas specific things
source ${REPO_PATH}/include/runtime/ENV ;

# Go back to repo root
cd ${REPO_PATH} ;
