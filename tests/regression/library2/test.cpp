//#include <cstdio>
//#include <cstdint>
//#include <vector>
#include <stdlib.h>
//#include <sys/mman.h>
#include "../../../include/wrapper.hpp"
#include "../../../include/pin_interface.hpp"
#include "library.hpp"


int main (int argc, char *argv[]){

  unsigned long b;
  uint64_t stateID = caratGetStateWrapper((char*)"main", 5);
  //startLogPinAll();
  b = func(42);
  caratReportStateWrapper(stateID);
  //stopLogPinAll();

  /*
  std::vector<Address*> address;
  address = retrieveAddressesVector();

  for(Address* a : address){
    fprintf(stderr, "Found an address in test.cpp of type: %d\n", a->type());
  }

  if(address.size() == 0) fprintf(stderr, "Found no addresses\n");
  else fprintf(stderr, "Found %ld addresses\n", address.size());
*/
  return 0;
}



