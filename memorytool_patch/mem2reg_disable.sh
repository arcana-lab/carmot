#!/bin/bash -e

# Get repo dir
REPO_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"/.. ;

cp ${REPO_PATH}/memorytool_patch/memorytool-mem2regdisabled ${REPO_PATH}/utils/memorytool ;

