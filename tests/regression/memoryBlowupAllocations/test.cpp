#include <cstdint>
#include <stdlib.h>
#include "../../../include/wrapper.hpp"

#define SIZE 10

class MyClass{
  public:
    int value = 42;
};

void func(void){
  //MyClass *input = new MyClass{};
  //MyClass *output = new MyClass{};
  //MyClass *cloneable = new MyClass{};
  //MyClass *transfer = new MyClass{};

  int input = 0;
  int output = 0;
  int cloneable = 0;
  int transfer = 0;

  uint64_t stateID = 0;
  for (int i = 0; i < SIZE; ++i){
    stateID = caratGetStateWrapper((char*)"func", 20);
    if (i == 0){
      output = 1;
    }
    int elem0 = input;
    cloneable = 3;
    transfer += 7;
    caratReportStateWrapper(stateID);
  }
  endStateInvocationWrapper(stateID);

  //delete input;
  //delete output;
  //delete cloneable;
  //delete transfer;

  return;
}

int main (int argc, char *argv[]){
  for (auto i = 0; i < 10000; ++i){
    func();
  }

  return 0;
}
