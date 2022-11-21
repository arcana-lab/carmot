#!/bin/bash

# Get repo dir 
CURR_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/.." ;

tests_path=${CURR_PATH} ;

# Check if test exists
if ! test -d ${tests_path} ; then
  echo "ERROR: ${tests_path} does not exist" ;
  exit 1 ;
fi

# Go to test dir
cd ${tests_path};

# Run all tests in each directory
for test_dir in `ls` ; do 
  if ! test -d ${test_dir} ; then
    continue
  fi

  if [ "${test_dir}" == "scripts" ]; then
    continue ;
  fi
  
  cd ${tests_path}/${test_dir}
  ${CURR_PATH}/scripts/run_test.sh ${CURR_PATH}/${test_dir} ;

  cd ${tests_path} ;
done

exit 0 ;
