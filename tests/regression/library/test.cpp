#include <cstdio>
#include <cstdint>
#include <vector>
#include <stdlib.h>
#include <sys/mman.h>
#include "../../../include//wrapper.hpp"
#include "./library.hpp"
#include "../../../include//pin_interface.hpp"



int main (int argc, char *argv[]){

  int n = atoi(argv[1]);
  int s = atoi(argv[2]);
  int w = atoi(argv[3]);

  char x[n];

  printf("Address of x: %p\n", x);

    startLogPinMemtrace();
    func4(x, n, s, w);
    stopLogPinMemtrace();
    
    std::vector<TouchedAddress*> addresses = retrieveAddressesVector();
    printAddressesVector(addresses, std::cout);
    printf("Lets go again! \n\n\n\n\n\n");
    startLogPinMemtrace();
    func4(x, n, s, w);
    stopLogPinMemtrace();
    
    addresses = retrieveAddressesVector();
    printAddressesVector(addresses, std::cout);

    printf("Lets go again! \n\n\n\n\n\n");
    startLogPinMemtrace();
    func4(x, n, s, w);
    stopLogPinMemtrace();

    AddressArray* array = retrieveAddressesArray();
    printAddressesArray(array, std::cout);

/*    
    startLogPinMemtrace();
    //func(3);
    //func2();
    //func3();
    
    stopLogPinMemtrace();
    
    addresses = retrieveAddresses();
    printAddresses(addresses, std::cout);

    startLogPinMemtrace();
    //func(3);
    //func2();
    //func3();
    stopLogPinMemtrace();
    
    addresses = retrieveAddresses();
    printAddresses(addresses, std::cout);
    
    startLogPinMemtrace();
    //func(3);
    //func2();
    //func3();
    stopLogPinMemtrace();
    
    addresses = retrieveAddresses();
    printAddresses(addresses, std::cout);
  */  
    return 0;
}



