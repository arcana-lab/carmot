#include <cstdint>
#include <stdlib.h>
#include "../../../include/wrapper.hpp"

int main (int argc, char *argv[]){
  int *a = (int*) calloc(3, sizeof(int)); // 0xaa

  for (auto i = 0; i < 16; ++i){
    uint64_t stateID = caratGetStateWrapper("main", 0);
    a[1] = 2;
    caratReportStateWrapper(stateID);

    int *b = (int*) realloc(a,(0x1<<i)*sizeof(int)); // 0xaa
    a = b;
  }
  //[0xaa] -> callstack of b

  int *b = (int*) realloc(a,4*sizeof(int)); // 0xaa

  return 0;
}
