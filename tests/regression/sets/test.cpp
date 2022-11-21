#include <cstdint>
#include <stdlib.h>
#include "../../../include/wrapper.hpp"

#define SIZE 10

class MyClass{
  public:
    int value = 42;
};

int main (int argc, char *argv[]){
  MyClass *input = new MyClass{};
  MyClass *output = new MyClass{};
  MyClass *cloneable = new MyClass{};
  MyClass *transfer = new MyClass{};

  for (int i = 0; i < SIZE; ++i){
    uint64_t stateID = caratGetStateWrapper((char*)"main", 5);
    int elem0 = input->value;
    output->value = 44;
    cloneable->value = 55;
    int elem1 = cloneable->value;
    transfer->value += 66;
    caratReportStateWrapper(stateID);
  }

  delete input;
  delete output;
  delete cloneable;
  delete transfer;

  return 0;
}
