#pragma once

#include<map>
#include<vector>
#include<iostream>
#include<unistd.h>

// type: 0 load/store, 1 malloc, 2 free

enum {
  PIN_TOUCHED = 0,
  PIN_MALLOC,
  PIN_FREE,
};

class Address{
  public:
    virtual int type() = 0;
};

class TouchedAddress : public Address{
  public:
    void* address;
    bool isWritten;
    TouchedAddress();
    TouchedAddress(void *address, bool isWritten);
    int type();
};

class MallocAddress : public Address{
  public:
    void* address;
    uint64_t size;
    MallocAddress();
    MallocAddress(void* address, uint64_t size);
    int type();
};

class FreeAddress : public Address{
  public:
    void* address;
    FreeAddress();
    FreeAddress(void* address);
    int type();
};

struct AddressArray{
  uint64_t size;
  TouchedAddress a[];
  //no member functions
};

extern "C" void startLogPinAll(void);

extern "C" void stopLogPinAll(void);

extern "C" void startLogPinMemtrace(void);

extern "C" void stopLogPinMemtrace(void);

extern "C" void startLogPinMalloc(void);

extern "C" void stopLogPinMalloc(void);
  
extern "C" void startLogPinFree(void);

extern "C" void stopLogPinFree(void);

extern "C" void retrieveAddress(void* address, void* isWritten);

extern "C" void retrieveNumAddresses(void* counter);

extern "C" void retrieveNextAddressType(int* type);

extern "C" void retrieveMallocAddress(void* address, void* size);

extern "C" void retrieveFreeAddress(void* address);


void printAddressesVector(std::vector<TouchedAddress*> addresses, std::ostream& out);

std::vector<Address*> retrieveAddressesVector(void);

