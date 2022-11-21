#include <cstdint>
#include <stdlib.h>
#include "../../../include/wrapper.hpp"

int main (int argc, char *argv[]){
  int *a = (int*) malloc(3*sizeof(int));
  a[0] = 1;
  a[1] = 3;
  a[2] = 73;

  for (int i = 0; i < 1000; ++i){
    uint64_t stateID = caratGetStateWrapper((char*)"main", 5);
    a[0] = a[0] + a[1];
    caratReportStateWrapper(stateID);
  }

  return 0;
}
