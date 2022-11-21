#!/bin/bash

if test $# -lt 1 ; then
  echo "USAGE: ./`basename ${0}` path/to/bitcode/file.bc" ;
  exit 1 ;
fi

# Get repo dir
REPO_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )" ;

# Run pass
${REPO_PATH}/bin/tag_functions_with_opt_none -emit-llvm -c ${1} ;
