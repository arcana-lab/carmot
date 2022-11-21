#include <cstdint>
#include <stdlib.h>
#include "../../../include/wrapper.hpp"

int main (int argc, char *argv[]){
  int *a = (int*) calloc(3, sizeof(int)); // 0xaa

  uint64_t stateID = caratGetStateWrapper("main", 0);
  a[1] = 2;
  caratReportStateWrapper(stateID);

  int *b = (int*) realloc(a,400000*sizeof(int)); // 0xaa

  return 0;
}
