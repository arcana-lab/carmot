#!/bin/bash

# Get repo dir
CURR_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../.." ;

test_path="$1" ;
# If a test dir is not provided, just return
if [ "${test_path}" == "" ] ; then
  echo "TEST: test not provided" ;
  exit 1 ;
fi

# Check if test exists
if ! test -d ${test_path} ; then
  echo "TEST: ${test_path} does not exist" ;
  exit 1 ;
fi

# Go to test dir
cd ${test_path};

# Check for Makefile, if there isn't one, make a symbolic link to the default one
if ! test -f ./Makefile ; then
  ln -s ../../scripts/Makefile Makefile ;
fi

# Compile test
make &> /dev/null ;
if [ "$?" != "0" ] ; then
  echo "TEST FAILED: COMPILATION ${test_path}" ;
  exit 1 ;
fi

# Run test
make run &> /dev/null ;
if [ "$?" != "0" ] ; then
  echo "TEST FAILED: EXECUTION ${test_path}" ;
  exit 1 ;
fi

# Check tool output against oracle
#cmp ./program.json ./program.json.oracle ;
#if [ "$?" != "0" ] ; then
#  echo "TEST FAILED: OUTPUT MISMATCH ${test_path}" ;
#fi

exit 0 ;
