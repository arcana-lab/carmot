#pragma once
#include <iostream>
#include <atomic>
#include <thread>
#include <unordered_map>
#include "Allocation.hpp"
#include "AllocationTable.hpp"
#include "HelperFunctions.hpp"
#include "State.hpp"
#include "./virgil/include/ThreadSafeSpinLockQueue.hpp"
#include "./virgil/include/ThreadSafeMutexQueue.hpp"

extern uint64_t NUM_SHADOWS;

//#define SHADOW_WINDOW 1073741824
//#define SHADOW_WINDOW 2097152
//#define SHADOW_WINDOW 4194304
//#define SHADOW_WINDOW 524288
//#define SHADOW_WINDOW 131072
//#define SHADOW_WINDOW 32768
extern uint64_t SHADOW_WINDOW; // GOOD
//#define SHADOW_WINDOW 8388608
//#define SHADOW_WINDOW 134217728 

extern uint64_t NUM_BATCHES;

extern int trackCycles;

extern std::unordered_map<uint64_t, Allocation*>* originToAllocMap;

typedef struct{
  uint64_t operation;	//0-FreeSlot 1-BeingAllocatedSlot, 3-allocAdd, 4-allocRemove, 5-escapeAdd, 6-stateAddWrite, 7- stateAddRead, 8-clean
  void* ptr;
  union{
    uint64_t size;
    State* statePtr;
  } sizeState;
  uint64_t stateToSet;
  State* affiliatedState;
  uint64_t information;
  
  std::atomic_uint64_t* batchEntriesLeft;
} shadowEntry;

typedef std::vector<shadowEntry*> texasWorkQueueType;


class Shadow{
  public:
    //This thread* will hold the thread that runs the function of processing the sub-batch
    std::thread* thread = nullptr; 
    //This queue will hold work that cannot be processed by the right thread
    texasWorkQueueType* overflowWork = nullptr;
    //This will be our local allocation table
    AllocationTable* localAllocationTable = nullptr;
    //This will point to a map of candidate state additions
    //This will point to the Shadow to the left of us (where we will pass work we cannot process)
    Shadow* leftShadow = nullptr;
    //This will point to the Shadow to the right of us (where we will accept work from and merge tables from)
    Shadow* rightShadow = nullptr;

    //This is a status bit to indicate to the left Shadow that it can merge our table into itself
    volatile int done = 0;
    //This will give us our tid
    int tid = 0;
    //This will provide us our index we start processing the batch
    uint64_t leftBound = 0;
    //This will provide us our index we stop processing the batch
    uint64_t rightBound = 0;
   

    //Constructor
    Shadow(int tid, AllocationTable* table, Shadow* left, Shadow* right);
   
    //Destructor
    ~Shadow(void);

    //This function will iterate through the respective indexes of the batch it needs to process
    //It then will process them accordingly
    void processShadowBatch(shadowEntry*);
    
    //These functions are the functions that processShadowBatch will call to process the entries
    void processAddAlloc(shadowEntry*);
    void processAddEscape(shadowEntry*);
    void processAddToState(shadowEntry*);
    void processFreeAllocation(shadowEntry*);
    void processStateBeginEnd(shadowEntry*);
    void processStateRegionSet(shadowEntry* shadEntry);
};


