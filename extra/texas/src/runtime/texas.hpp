#pragma once
#include <atomic>
#include <shared_mutex>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <tbb/concurrent_hash_map.h>
#include <unordered_map>
#include <map>
#include <bits/stdc++.h> 
#include <utility>
#include <unordered_set>
#include <set>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstdlib>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <ucontext.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <sys/time.h>
#include <malloc.h>
#include "./virgil/include/ThreadSafeSpinLockQueue.hpp"
#include "./virgil/include/ThreadSafeMutexQueue.hpp"
#include "boostlibs.hpp"

#include "HelperFunctions.hpp"
#include "Allocation.hpp"
#include "AllocationTable.hpp"
#include "State.hpp"
#include "Shadow.hpp"




#ifndef __ALLOC_ENTRY__
#define __ALLOC_ENTRY__
#endif//allocEntry ifndef



//---------------------------ALLOCATION TABLE MODIFIERS-------------------------------------------



extern bool inRT;
#define ENTER_RT if(inRT){return;} inRT = true; if(allocationMap == nullptr){texas_init();}
#define EXIT_RT inRT=false; return;

//Addr Escaping To , allocAddr, length
extern Allocation* StackEntry;
extern uint64_t rsp;



extern std::unordered_map<uint64_t, Allocation*>* originToAllocMap;

extern AllocationTable* allocationMap;

//These functions will add allocations to the tracking table (allocationMap)
void AddToAllocationTable(void* addr, uint64_t size, uint64_t id);
void AddToAllocationTable(void* addr, uint64_t size, uint64_t id, State* curState);
void AddCallocToAllocationTable(void* addr, uint64_t numElements, uint64_t sizeOfElement, uint64_t id);
void AddCallocToAllocationTable(void* addr, uint64_t numElements, uint64_t sizeOfElement, uint64_t id, State* curState);
void HandleReallocInAllocationTable(void* addr, void* newAddr, uint64_t size, uint64_t id);
void HandleReallocInAllocationTable(void* addr, void* newAddr, uint64_t size, uint64_t id, State* curState);

//This function will track pointers that alias an allocation
void AddToEscapeTable(void*);

//This function will remove allocations from being tracked
void RemoveFromAllocationTable(void*);
void RemoveFromAllocationTable(void*, State*);

//This is a dummy function that will insert a redundant op into the batch. Good for timing things
void AddRedundantOp(void);


//This function will catch the runtime up to current execution and then look for a cycle in the allocationMap
bool FindCycleInAllocationMap(volatile int* sync);
bool FindCycleInAllocationMap(volatile int* sync, uint64_t);


//-------------------------------STATE TRACKING---------------------------------------------------


extern uint64_t currentStateID;
extern State* activeState;


uint64_t GetState(char* funcName, uint64_t lineNum, uint64_t temporalTracking); 
uint64_t EndState(uint64_t stateID);
uint64_t EndStateInvocation(uint64_t);

extern "C" uint64_t CGetState(char* fN, uint64_t lN, uint64_t temporalTracking);
extern "C" uint64_t CEndState(uint64_t sID);

//This will iterate the stateMap and call state->ReportState() for every state
void ReportStates();

//This will be put ahead of all store/load instructions during tracking to help build the current state
void AddToState(void* allocPtr, int rw, uint64_t id);
extern "C" uint64_t CAddToState(void* allocPtr, uint64_t id);
void AddToStateMulti(void* allocPtr1, void* allocPtr2, int rw, int rw2, uint64_t id);
void AddToStateShadow(void* allocPtr, int rw, uint64_t id);
void SetStateRegion(void* startPtr, uint64_t lengthInBytes, uint64_t stateID, uint64_t stateToSet, uint64_t id);


//-----------------------------SHADOW PROFILER------------------------------------

//This global will slow things down significantly
extern int disableOptimizations;
extern int trackTransfer;
extern int trackCloneable;
extern int trackInput;
extern int trackOutput;

//This struct the array of things to process (shadowEntries) wrapped with metainfo
typedef struct{
  uint64_t numEntries;
  shadowEntry* shadowEntries;
  uint64_t status; //0-New Batch, 1-Cleaned Batch, 2-Kill
  uint64_t batchNumber;
  std::atomic_uint64_t batchEntriesRemaining;
} shadowBatch;

typedef MARC::ThreadSafeSpinLockQueue<shadowBatch*> texasWorkEntriesQueueType;
typedef MARC::ThreadSafeSpinLockQueue<shadowBatch*> cleanedTexasWorkEntriesQueueType;

//Queue of batches
typedef MARC::ThreadSafeSpinLockQueue<shadowEntry*> texasReadyBatchesQueueType;

//This is a pointer to the current batch being populated by the main thread
extern shadowEntry* texasWorkEntries;

//Batches that have been filled but not processed go here
extern texasWorkEntriesQueueType* texasWorkEntriesQueue;

//Batches that have been processed and ready for reuse go here
extern texasReadyBatchesQueueType* texasReadyBatchEntriesQueue;

//Batches that have been cleaned go here
extern cleanedTexasWorkEntriesQueueType* cleanedTexasWorkEntriesQueue;

//This variable indicates our index in the current batch. This will max out when it fills up the batch completely.
//It then resets to 0
extern uint64_t slotStart;

//This will give us batch IDs in order they are processed
extern uint64_t curBatchNumber;

//This will give us the batch currently being processed
extern uint64_t servingBatchNumber;



enum{
  NEW_BATCH = 0,
  CLEANED_BATCH,
  KILL_BATCH
};

//This will give an entry to the main program to populate
shadowEntry* getFreeSlot();

//This function will process a shadow batch from creation completely in serial
void serialProcessShadowBatch();

extern volatile State* oldState;
//This function will clean a batch for redundant operations
void cleanBatch(shadowBatch* entry, volatile uint64_t* progress);

//This function will run the shadow thread
void shadowRun(Shadow* shadowThread, shadowBatch* batch, volatile uint64_t* done);

//This function will do the equivalent of processShadowBatches(), but with only a single thread
void singleShadowProcessShadowBatches();

//This function is what the master shadow thread will run
void processShadowBatches(void);
extern std::thread* shadowMasterThread;

void batchFull(bool);

//-----------------------------INITIALIZATION-------------------------------------

void texas_init();
class texasStartup;


//----------------------------FINISHING/REPORTING---------------------------------

//This will report out the stats of a program run
void ReportStatistics();

//This will report a histogram relating to escapes per allocation.
void HistogramReport();


