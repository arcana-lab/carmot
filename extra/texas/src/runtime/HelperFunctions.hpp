#pragma once
#include <stdint.h>

enum{
  FREE_SLOT = 0,
  ALLOCATING,
  ALLOC_ADD,
  ALLOC_FREE,
  ESCAPE_ADD,
  STATE_ADD_READ,
  STATE_ADD_WRITE,
  STATE_ADD_INPUT,
  STATE_ADD_IO,
  STATE_ADD_CIO,
  STATE_ADD_TIO,
  STATE_ADD_OUTPUT,
  STATE_ADD_CO,
  STATE_ADD_TO,
  STATE_BEGIN,
  STATE_END,
  STATE_REGION_SET,
  THREAD_KILL,
  REDUNDANT,
  STATE_COMMIT,
  DETECT_CYCLE,
} operations;



#ifdef DEBUG_OUTPUT
#define DEBUG(S, ...) fprintf(stderr, "carat_user: " S, ##__VA_ARGS__)
#else 
#define DEBUG(S, ...) 
#endif

int64_t doesItAlias(void* allocAddr, uint64_t length, uint64_t escapeVal);

uint64_t GetOffset(void* baseAddr, void* offsetAddr);

uint64_t getrsp();

uint64_t rdtsc (void);

