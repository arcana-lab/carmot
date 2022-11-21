#!/bin/bash

# Get repo dir
CURR_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/.." ;

tests_path=${CURR_PATH} ;

# Check if test exists
if ! test -d ${tests_path} ; then
  echo "ERROR: ${tests_path} does not exist" ;
  exit 1 ;
fi

# Go to regression test dir
cd ${tests_path};

# Clean all tests in the directory
for test in `ls` ; do
  if ! test -d ${test} ; then
    continue ;
  fi

  if [ "${test}" == "scripts" ] ; then
    continue ;
  fi

  cd ${tests_path}/${test} ;
  make clean &> /dev/null ;
  # test -h ./Makefile && rm ./Makefile ; # If Makefile is a symbolic link, then remove it

  cd ${tests_path};
done

exit 0 ;
