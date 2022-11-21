#include <cstdint>
#include <stdlib.h>
#include <string>
#include "../../../include/wrapper.hpp"

class C {
public:
  int num;
  int num2;
};

class C2 {
public:
  char ch;
  C objInClass;
};

int main (int argc, char *argv[]){
  C *obj = new C();
  C2 *obj2 = new C2();
  C objStack{};

  uint64_t stateID = caratGetStateWrapper("main", 0);
  obj->num = 4;
  objStack.num2 = 6;
  obj2->objInClass.num = 7;
  caratReportStateWrapper(stateID);

  return 0;
}
