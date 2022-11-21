#!/bin/bash

# Get repo dir
REPO_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/../.." ;

tests_path=${REPO_PATH}/tests ;

# Check if test exists
if ! test -d ${tests_path} ; then
  echo "ERROR: ${tests_path} does not exist" ;
  exit 1 ;
fi

# Go to regression test dir
cd ${tests_path}/regression;

# Copy program.json output into oracle
output_name="program.json" ;
for test in `ls` ; do
  if ! test -d ${test} ; then
    continue ;
  fi

  if [ "${test}" == "scripts" ] ; then
    continue ;
  fi

  output_path=./${test}/${output_name} ;

  # Check if program.json exists, if it doesn't run the test
  if ! test -f ${output_path} ; then
    ${REPO_PATH}/tests/scripts/run_test.sh ${test} ;
  fi

  # Check if after running the test program.json exists, if it doesn't something wrong happened
  if ! test -f ${output_path} ; then
    echo "ERROR: oracle output cannot be generated" ;
    continue ;
  fi

  cp ${output_path} ${output_path}.oracle ;
done

exit 0 ;
