#include <cstdint>
#include <stdlib.h>
#include "../../../include/wrapper.hpp"

#define SIZE 10

int main (int argc, char *argv[]){
  int a[SIZE];

  int i;

  for (i = 0 ; i < SIZE; ++i){
    uint64_t stateID = caratGetStateWrapper("main", 0);
    a[i] = i;
    caratReportStateWrapper(stateID);
  }

  return 0;
}
