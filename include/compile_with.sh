#!/bin/bash

flags="" ;
for elem in $@ ; do
  flags="${flags} -D${elem}" ;
done

echo ${flags} ;
make clean ;
make CPPFLAGS="${flags}" ;
