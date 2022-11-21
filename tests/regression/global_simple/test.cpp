#include <cstdint>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <fstream>
#include "../../../include//wrapper.hpp"

void *p = nullptr;
int a = 234;
int *m = (int*) malloc(3*sizeof(int));


void doNothing(void *arg){
  p = arg;
  return;
}

int main (int argc, char *argv[]){
  uint64_t stateID = caratGetStateWrapper((char*)"main", 0);
  a = 235;
  m[0] = 1;
  doNothing(m);
  caratReportStateWrapper(stateID);

  free(m);

  return 0;
}
