#include <cstdint>
#include <stdlib.h>
#include "../../../include//wrapper.hpp"

class C {
public:
  int num;
  int num2;
};

int main (int argc, char *argv[]){
  C *obj = new C();

  uint64_t stateID = caratGetStateWrapper("main", 0);
  delete obj;
  caratReportStateWrapper(stateID);

  return 0;
}
