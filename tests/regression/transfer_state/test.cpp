#include <cstdint>
#include <stdlib.h>
#include "../../../include/wrapper.hpp"

#define SIZE 10

int main (int argc, char *argv[]){
  int a[SIZE];

  for (auto i = 1; i < SIZE; ++i){
    uint64_t stateID = caratGetStateWrapper("main", 0);
    a[i] = a[i-1] + 42;
    caratReportStateWrapper(stateID);
  }

  return 0;
}
