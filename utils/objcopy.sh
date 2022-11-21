#!/bin/bash

# Get args
prefix=${1} ;
pathToObjectFile=${2} ;

# Get repo dir
REPO_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )/.." ;

suffix="symbol" ;

# Get exported symbols
exportedSymbolsFileName="exported_symbols_${pathToObjectFile}.${suffix}" ;
rm -f ${exportedSymbolsFileName} ; # remove existing file, just in case
#nm ${pathToObjectFile} | grep -i -F " t " | awk -v prefix=${prefix} '{print $3 " " prefix$3}' > ./${exportedSymbolsFileName} ;
nm --defined-only ${pathToObjectFile} | awk -v prefix=${prefix} '{print $3 " " prefix$3}' > ./${exportedSymbolsFileName} ;

# Remove symbol main
exportedSymbolsNoMainFileName="exported_symbols_no_main_${pathToObjectFile}.${suffix}" ;
rm -f ${exportedSymbolsNoMainFileName} ; # remove existing file, just in case
touch ${exportedSymbolsNoMainFileName} ; # create it
while IFS= read -r line ; do
  if [ "${line}" == "main ${prefix}main" ] ; then
    continue ;
  fi

  echo ${line} >> ${exportedSymbolsNoMainFileName} ;
done < "${exportedSymbolsFileName}"

# Change only exported symbols
llvm-objcopy --redefine-syms ${exportedSymbolsNoMainFileName} ${pathToObjectFile} ;
