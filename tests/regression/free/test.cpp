#include <cstdint>
#include <stdlib.h>
#include "../../../include//wrapper.hpp"

int main (int argc, char *argv[]){
  int *a = (int*) malloc(3*sizeof(int));

  uint64_t stateID = caratGetStateWrapper("main", 0);
  free(a);
  caratReportStateWrapper(stateID);

  return 0;
}
