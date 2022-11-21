#include <cstdint>
#include <stdlib.h>
#include "../../../include/wrapper.hpp"

int main (int argc, char *argv[]){
  int *a = (int*) calloc(3, sizeof(int));

  uint64_t stateID = caratGetStateWrapper("main", 8);
  uint64_t stateID2 = caratGetStateWrapper("main", 9);
  a[1] = 2;
  if (a[1] == 2){
    uint64_t stateID3 = caratGetStateWrapper("main", 12);
    a[0] = 3;
    for (auto i = 0; i < 10; ++i){
      uint64_t stateID4 = caratGetStateWrapper("main", 15);
      a[2] = 7;
      caratReportStateWrapper(stateID4);
    }
    caratReportStateWrapper(stateID3);
  }
  caratReportStateWrapper(stateID2);
  caratReportStateWrapper(stateID);

  uint64_t stateID5 = caratGetStateWrapper("main", 24);
  a[2] = 8;
  caratReportStateWrapper(stateID5);

  return 0;
}
