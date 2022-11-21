#include <cstdint>
#include <stdlib.h>
#include "../../../include/wrapper.hpp"

int main (int argc, char *argv[]){
  int a[10];

  uint64_t stateID = caratGetStateWrapper("main", 0);
  a[1] = 2;
  caratReportStateWrapper(stateID);

  return 0;
}
