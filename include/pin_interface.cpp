#include "pin_interface.hpp"

TouchedAddress::TouchedAddress():address{nullptr},isWritten{false} {}
TouchedAddress::TouchedAddress(void *address, bool isWritten):address{address},isWritten{isWritten} {}
int TouchedAddress::type(){ return 0; }

MallocAddress::MallocAddress():address{nullptr},size{0} {}
MallocAddress::MallocAddress(void* address, uint64_t size):address{address},size{size} {}
int MallocAddress::type(){ return 1; }

FreeAddress::FreeAddress():address{nullptr} {}
FreeAddress::FreeAddress(void* address):address{address} {}
int FreeAddress::type(){ return 2; }

extern "C" void startLogPinAll(void){}

extern "C" void stopLogPinAll(void){}

extern "C" void startLogPinMemtrace(void){}

extern "C" void stopLogPinMemtrace(void){}

extern "C" void startLogPinMalloc(void){}

extern "C" void stopLogPinMalloc(void){}
  
extern "C" void startLogPinFree(void){}

extern "C" void stopLogPinFree(void){}

extern "C" void retrieveNextAddressType(int* type){}

extern "C" void retrieveAddress(void* address, void* isWritten){}

extern "C" void retrieveNumAddresses(void* counter){}

extern "C" void retrieveMallocAddress(void* address, void* size){}

extern "C" void retrieveFreeAddress(void* address){}


void printAddressesVector(std::vector<TouchedAddress*> addresses, std::ostream& out){
  printf("Printing application list of addresses from vector: \n");
  for(TouchedAddress* t : addresses){
    out << "Address " << std::hex << (uint64_t)t->address << ((t->isWritten) ? " was written" : " was read") << std::endl;  
  }
}

std::vector<Address*> retrieveAddressesVector(void){
  std::vector<Address*> addresses;
  uint64_t i = 0;
  int type = -1;
  //printf("Application starting loop...\n");
  while(true){

    retrieveNumAddresses(&i);
    if(i == 0){
      //printf("Application all addresses found, ending loop...\n");
      break;
    }
    
    retrieveNextAddressType(&type);
    if(type == 0){
      TouchedAddress* curr = new TouchedAddress();
      retrieveAddress(&curr->address, &curr->isWritten);
      addresses.push_back(curr);
    }
    else if(type == 1){
      MallocAddress* curr = new MallocAddress();
      retrieveMallocAddress(&curr->address, &curr->size);
      addresses.push_back(curr);
    }
    else if(type == 2){
      FreeAddress* curr = new FreeAddress();
      retrieveFreeAddress(&curr->address);
    }
    else{
      printf("Error! Unknown type\n");
    }
  }

  return addresses;
}
