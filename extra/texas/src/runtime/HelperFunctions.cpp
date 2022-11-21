#include "HelperFunctions.hpp"

int64_t doesItAlias(void* allocAddr, uint64_t length, uint64_t escapeVal){
  uint64_t blockStart = (uint64_t) allocAddr;
  if(escapeVal >= blockStart && escapeVal < (blockStart + length)){
    return escapeVal - blockStart;
  }
  else{
    return -1;
  }
}

uint64_t GetOffset(void* baseAddr, void* offAddr){
  return (uint64_t)offAddr - (uint64_t)baseAddr;
}

uint64_t getrsp(){
  uint64_t retVal;
  __asm__ __volatile__("movq %%rsp, %0" : "=a"(retVal) : : "memory");
  return retVal;
}

uint64_t rdtsc (void)
{
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
}


