#include <cstdint>
#include <stdlib.h>
#include <string>
#include "../../../include/wrapper.hpp"

class C {
public:
  int num;
  int num2;
  std::string myString;
  std::string myString2;

  C(int numArg){
    this->num = numArg;
    this->num2 = 666;
    this->myString = "ciao";
  }
};

void myF(C &objArg){
  objArg.num2 = 777;

  return;
}

int main (int argc, char *argv[]){
  C *obj = new C(argc);

  uint64_t stateID = caratGetStateWrapper("main", 0);
  myF(*obj);
  sizeof(*obj);
  caratReportStateWrapper(stateID);

  return 0;
}
