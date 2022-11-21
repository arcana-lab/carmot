#include "texas.hpp"
#include <atomic>
#include <cstdlib>
#include <unordered_map>




bool inRT = false;
//
//
//-----------------------------ALLOCATION TABLE MODIFIERS--------------------------------------
//
//

AllocationTable* allocationMap;

Allocation* StackEntry;
uint64_t rsp = 0;


void AddToAllocationTable(void* address, uint64_t length, uint64_t origin){
  ENTER_RT;
  auto* entry = getFreeSlot();
  entry->operation = ALLOC_ADD;
  entry->ptr = address;
  entry->sizeState.size = length;
  entry->information = origin;
  entry->affiliatedState = activeState;
  EXIT_RT;
}

void AddCallocToAllocationTable(void* address, uint64_t len, uint64_t sizeOfEntry, uint64_t origin){
  AddToAllocationTable(address, len*sizeOfEntry, origin);
}

void HandleReallocInAllocationTable(void* address, void* newAddress, uint64_t length, uint64_t origin){
  RemoveFromAllocationTable(address);
  AddToAllocationTable(newAddress, length, origin);
}

//This function will remove an address from the allocation from a free() or free()-like instruction being called
void RemoveFromAllocationTable(void* address){
  ENTER_RT;
  auto* entry = getFreeSlot();
  entry->operation = ALLOC_FREE;
  entry->ptr = address;
  entry->affiliatedState = activeState;
  EXIT_RT;
}


//THESE VERSIONS OF THE FUNCTION CALLS WILL ATTACH STATE METADATA TO THE ALLOCATIONS WHICH IMPLY STATE COMMITS MAY NOT NEED TO HAPPEN


void AddToAllocationTable(void* address, uint64_t length, uint64_t origin, State* state){
  ENTER_RT;
  auto* entry = getFreeSlot();
  entry->operation = ALLOC_ADD;
  entry->ptr = address;
  entry->sizeState.size = length;
  entry->affiliatedState = state;
  entry->information = origin;
  EXIT_RT;
}

void AddCallocToAllocationTable(void* address, uint64_t len, uint64_t sizeOfEntry, uint64_t origin, State* state){
  AddToAllocationTable(address, len*sizeOfEntry, origin, state);
}

void HandleReallocInAllocationTable(void* address, void* newAddress, uint64_t length, uint64_t origin, State* state){
  RemoveFromAllocationTable(address, state);
  AddToAllocationTable(newAddress, length, origin, state);
}

//This function will remove an address from the allocation from a free() or free()-like instruction being called
void RemoveFromAllocationTable(void* address, State* state){
  ENTER_RT;
  auto* entry = getFreeSlot();
  entry->operation = ALLOC_FREE;
  entry->ptr = address;
  entry->affiliatedState = state;
  EXIT_RT;
}
void AddToEscapeTable(void* addressEscaping){
  if(!trackCycles){
    return;
  }
  ENTER_RT;
  auto* entry = getFreeSlot();
  entry->operation = ESCAPE_ADD;
  entry->ptr = addressEscaping;
  EXIT_RT;
}



//This function will catch the runtime up to current execution and then look for a cycle in the allocationMap
bool FindCycleInAllocationMap(volatile int* sync){
  auto* entry = getFreeSlot();
  entry->ptr = (void*) sync;
  entry->operation = DETECT_CYCLE;
  entry->sizeState.size = -1;
  //End the batch right now and push it
  batchFull(false);

  std::cerr << "PUSHED CYCLE FIND TO BATCH\n";

  return false;
}

bool FindCycleInAllocationMap(volatile int* sync, uint64_t stateID){
  auto* entry = getFreeSlot();
  entry->ptr = (void*) sync;
  entry->operation = DETECT_CYCLE;
  entry->sizeState.size = stateID;
  //End the batch right now and push it
  batchFull(false);

  std::cerr << "PUSHED CYCLE FIND TO BATCH\n";

  return false;
}
//
//
//----------------------------STATE TRACKING-----------------------------------------------------
//
//


uint64_t currentStateID = 0;
State* activeState = nullptr;


uint64_t GetState(char* funcName, uint64_t lineNum, uint64_t temporalTracking){
  bool found = false;
  //Look for state before making a new one
  for(auto a : stateMap){
    if( (strcmp(a.second->functionName.c_str(), funcName) == 0) && a.second->lineNumber == lineNum){
      //printf("Found state\n");
      activeState = a.second;
      found = true;
      break;    
    }
  }

  if(!found){
    activeState = new State(funcName, lineNum, temporalTracking, currentStateID, activeState);
    currentStateID++;
    //printf("making new state: %p\n", activeState);
    stateMap.insert_or_assign(activeState->stateID, activeState);
  }

  //This is a configurable option to get naive state size at invocation
  if(stateReductionTracking){
    uint64_t naiveSize = allocationMap->GetMemFootprint();
    if(naiveSize > activeState->naiveStateSize){
      activeState->naiveStateSize = naiveSize;
    }
  }

  auto* entry = getFreeSlot();
  entry->operation = STATE_BEGIN;
  entry->sizeState.statePtr = activeState;

  //DEBUG("Returning state %lu\n", activeState->stateID);
  return activeState->stateID;
}

uint64_t EndState(uint64_t stateID){
  if(activeState != nullptr){

    if(activeState->stateID != stateID){
      DEBUG("Ending improper state\n"); 
    }
    auto* entry = getFreeSlot();
    entry->operation = STATE_END;
    entry->sizeState.statePtr = activeState;
    activeState = activeState->parent;
  }
  else{
    return 1;
  }
  return 0;
}

uint64_t EndStateInvocation(uint64_t stateID){
  auto a = stateMap.find(stateID);
  if(a == stateMap.end()){
    return -1;
  }
  auto* entry = getFreeSlot();
  entry->operation = STATE_COMMIT;
  entry->sizeState.statePtr = a->second;
  //End the batch right now and push it
  batchFull(false);
  return 0;
}

void AddToStateMulti(void* allocPtr1, void* allocPtr2, int rw, int rw2, uint64_t id){
  AddToState(allocPtr1, rw, id);
  if(allocPtr2 != 0){
    AddToState(allocPtr2, rw2, id);
  } 
}

void AddRedundantOp(){
  ENTER_RT;
  auto* entry  = getFreeSlot();

  entry->information = 0;
  entry->operation = REDUNDANT;
  entry->ptr = 0;
  entry->sizeState.size = 0;

  EXIT_RT;
}

//Read = 0
//Write = 1
void AddToState(void* allocPtr, int rw, uint64_t id){
  ENTER_RT;
  //printf("Add to state %p\n", allocPtr);
  if(activeState == nullptr){
    EXIT_RT;
  }
  AddToStateShadow(allocPtr, rw, id);
  EXIT_RT;
}


void AddToStateShadow(void* allocPtr, int rw, uint64_t id){
  auto* entry = getFreeSlot();
  entry->operation = STATE_ADD_READ + rw;
  entry->ptr = allocPtr;
  entry->information = id;
}

void SetStateRegion(void* startPtr, uint64_t lengthInBytes, uint64_t stateID, uint64_t stateToSet, uint64_t id){
  ENTER_RT;
  auto* entry  = getFreeSlot();

  entry->information = id;
  entry->operation = STATE_REGION_SET;
  entry->ptr = startPtr;
  entry->affiliatedState = stateMap[stateID];
  entry->sizeState.size = lengthInBytes;
  entry->stateToSet = (char)stateToSet;

  EXIT_RT;
}



void ReportStates(){
  for(auto state : stateMap){
    state.second->ReportState();
  }
}

//
//
//-----------------SHADOW PROFILER------------------------------------
//
//
//


int disableOptimizations = 0;
int trackTransfer = 1;
int trackCloneable = 1;
int trackInput = 1;
int trackOutput = 1;


std::thread* shadowMasterThread;

uint64_t slotStart = 0;
texasWorkEntriesQueueType* texasWorkEntriesQueue = nullptr;
texasReadyBatchesQueueType* texasReadyBatchEntriesQueue = nullptr;
cleanedTexasWorkEntriesQueueType* cleanedTexasWorkEntriesQueue = nullptr;
shadowEntry* texasWorkEntries = nullptr;

uint64_t curBatchNumber = 0;
uint64_t servingBatchNumber = 0;
uint64_t cleaningBatchNumber = 0;


void serialProcessShadowBatch(shadowBatch* batchToProcess){
  //First you clean it
  volatile uint64_t dummy;
  cleanBatch(batchToProcess, &dummy);

  //Now create a shadow for ourself
  Shadow* worker = new Shadow(0, allocationMap, nullptr, nullptr);

  //find bounds first
  DEBUG("shadowRun called for tid %d, batch %lu\n", worker->tid, batchToProcess->batchNumber);
  uint64_t subBatchSize = batchToProcess->numEntries;
  worker->leftBound = 0;
  worker->rightBound = subBatchSize;


  //Now that we have bounds, we can execute the batch
  worker->processShadowBatch(batchToProcess->shadowEntries);
  DEBUG("shadowRun DONE for tid %d, batch %lu\n", worker->tid, batchToProcess->batchNumber);
  delete worker;
  texasReadyBatchEntriesQueue->push(batchToProcess->shadowEntries);
  free(batchToProcess); 
}

void batchFull(bool capacityFill){
  shadowBatch* newBatch = (shadowBatch*) malloc(sizeof(shadowBatch));
  if(capacityFill){
    newBatch->numEntries = SHADOW_WINDOW;
  }
  else{
    newBatch->numEntries = slotStart ;
  }
  newBatch->status = NEW_BATCH;
  newBatch->shadowEntries = texasWorkEntries;
  newBatch->batchNumber = curBatchNumber;
  DEBUG("Created batch %lu\n", curBatchNumber);
  curBatchNumber++;
  if(disableOptimizations){
    serialProcessShadowBatch(newBatch); 
  }
  else{
    texasWorkEntriesQueue->push(newBatch);
  }
  slotStart = 0;
  texasReadyBatchEntriesQueue->waitPop(texasWorkEntries);

}

inline shadowEntry* __attribute__((always_inline)) getFreeSlot(void){
  if(slotStart >= SHADOW_WINDOW){
    batchFull(true);
  }
  shadowEntry* returner = &(texasWorkEntries[slotStart]);
  slotStart++;
  return returner; 
}


void shadowRun(Shadow* shadowThread, shadowBatch* batch, volatile uint64_t* done){

  DEBUG("shadowRun called for tid %d, batch %lu\n", shadowThread->tid, batch->batchNumber);
  //find bounds first
  uint64_t subBatchSize = (batch->numEntries / NUM_SHADOWS);
  uint64_t extraWork = batch->numEntries % NUM_SHADOWS;
  if(subBatchSize == 0){
    shadowThread->leftBound = 0;
    shadowThread->rightBound = 0;
    if(shadowThread->tid == 0){
      shadowThread->rightBound += extraWork;
    }
  }
  else{
    shadowThread->leftBound = shadowThread->tid * subBatchSize;
    shadowThread->rightBound = (((shadowThread->tid) + 1) * subBatchSize)- 1;
    if(shadowThread->tid + 1  == NUM_SHADOWS){
      shadowThread->rightBound += extraWork;
    }
  }


  //Now that we have bounds, we can execute the batch
  shadowThread->processShadowBatch(batch->shadowEntries);
  if(shadowThread->tid == 0){
    //Free the batch
    free(batch->shadowEntries);
    free(batch);
    *done = 2; 
  }
  DEBUG("shadowRun DONE for tid %d, batch %lu\n", shadowThread->tid, batch->batchNumber);
}

volatile State* oldState = nullptr;

//This will iterate through a batch and prep it for building the appropriate sets
void cleanBatch(shadowBatch* entry, volatile uint64_t* progress){
  State* batchState = (State*)oldState;

  shadowEntry* shadEntry = entry->shadowEntries;
  for(uint64_t i = 0; i < entry->numEntries; i++){
    //PUSH WORK INTO texasWorkQueue
    switch(shadEntry[i].operation){
      case STATE_ADD_READ:
      case STATE_ADD_WRITE:
        {
          if(batchState == nullptr){
            if(trackCycles){
              shadEntry[i].operation = REDUNDANT; 
            }
            continue;
          }
          void* ptr = shadEntry[i].ptr;
          auto state = batchState->statePtrMap.find(ptr);
          shadEntry[i].sizeState.statePtr = batchState;
          if(state != batchState->statePtrMap.end()){
            //This pointer exists in the previous iterations
            switch(state->second){
              case STATE_ADD_INPUT:
                if(shadEntry[i].operation == STATE_ADD_WRITE){
                  batchState->statePtrMap.insert_or_assign(ptr, STATE_ADD_IO); 
                  batchState->currentPtrIterUpgrades.insert(ptr);
                }
                continue;
              case STATE_ADD_IO:
                if(shadEntry[i].operation == STATE_ADD_READ){
                  if(batchState->currentPtrIterUpgrades.count(ptr)){ continue;}
                  batchState->statePtrMap.insert_or_assign(ptr, STATE_ADD_TIO); 
                }
                else{
                  if(batchState->currentPtrIterUpgrades.count(ptr)){ continue;}
                  batchState->statePtrMap.insert_or_assign(ptr, STATE_ADD_CIO); 
                }
                batchState->currentPtrIterUpgrades.insert(ptr);
                continue;
              case STATE_ADD_CIO:
                if(shadEntry[i].operation == STATE_ADD_READ){
                  if(batchState->currentPtrIterUpgrades.count(ptr)){ continue;}
                  batchState->statePtrMap.insert_or_assign(ptr, STATE_ADD_TIO); 
                }
                batchState->currentPtrIterUpgrades.insert(ptr);
                continue;
              case STATE_ADD_TIO:
                batchState->currentPtrIterUpgrades.insert(ptr);
                continue;
              case STATE_ADD_OUTPUT:
                if(shadEntry[i].operation == STATE_ADD_READ){
                  if(batchState->currentPtrIterUpgrades.count(ptr)){ continue;}
                  batchState->statePtrMap.insert_or_assign(ptr, STATE_ADD_TO); 
                }
                else{
                  if(batchState->currentPtrIterUpgrades.count(ptr)){ continue;}
                  batchState->statePtrMap.insert_or_assign(ptr, STATE_ADD_CO); 
                }
                batchState->currentPtrIterUpgrades.insert(ptr);
                continue;
              case STATE_ADD_CO:
                if(shadEntry[i].operation == STATE_ADD_READ){
                  if(batchState->currentPtrIterUpgrades.count(ptr)){ continue;}
                  batchState->statePtrMap.insert_or_assign(ptr, STATE_ADD_TO); 
                }
                batchState->currentPtrIterUpgrades.insert(ptr);
                continue;
              case STATE_ADD_TO:
                batchState->currentPtrIterUpgrades.insert(ptr);
                continue;
            }
          }
          else{
            auto iterState = batchState->statePtrIterMap.find(ptr);
            if(iterState != batchState->statePtrIterMap.end()){
              if(iterState->second == STATE_ADD_INPUT && shadEntry[i].operation == STATE_ADD_WRITE){
                batchState->statePtrIterMap.insert_or_assign(ptr, STATE_ADD_IO);
              }
              continue;
            }
            if(shadEntry[i].operation == STATE_ADD_WRITE){
              batchState->statePtrIterMap.insert({ptr, STATE_ADD_OUTPUT});
            }
            else if(shadEntry[i].operation == STATE_ADD_READ){
              batchState->statePtrIterMap.insert({ptr, STATE_ADD_INPUT});
            }
            continue;
          }
        }
        break;
      case STATE_END:
        //Iterate through the statePtrIterMap, and elevate them to statePtrMap
        batchState = shadEntry[i].sizeState.statePtr;
        for(auto statePtrIterMapEntry : batchState->statePtrIterMap){
          batchState->statePtrMap.insert(statePtrIterMapEntry);
        }
        //Invalidate end so it isn't processed
        batchState = shadEntry[i].sizeState.statePtr->parent;
        oldState = batchState;
        shadEntry[i].operation = REDUNDANT; 
        continue;
      case STATE_BEGIN:
        //Clear the iterCandidates
        batchState = shadEntry[i].sizeState.statePtr;
        oldState = batchState;
        batchState->statePtrIterMap.clear(); 
        batchState->currentPtrIterUpgrades.clear();
        //Invalidate the operation
        shadEntry[i].operation = REDUNDANT; 
        continue;
      default:
        continue;
    }
  }

  //Now we can go through the shadowEntries and reassign opperations based on what set they belong to and if we track them. 
  for(uint64_t i = 0; i < entry->numEntries; i++){
    if(entry->shadowEntries[i].operation == STATE_ADD_READ || entry->shadowEntries[i].operation == STATE_ADD_WRITE){
      auto statePtrMapEntry = entry->shadowEntries[i].sizeState.statePtr->statePtrMap.find(entry->shadowEntries[i].ptr);
      if(statePtrMapEntry == entry->shadowEntries[i].sizeState.statePtr->statePtrMap.end()){
        if(!trackCycles){
          entry->shadowEntries[i].operation = REDUNDANT;
        }
        continue;
      }
      switch(statePtrMapEntry->second){
        case STATE_ADD_INPUT:
          if(trackInput){
            entry->shadowEntries[i].operation = statePtrMapEntry->second;
            continue;
          }
          if(!trackCycles){
            entry->shadowEntries[i].operation = REDUNDANT;
          }
          continue;
        case STATE_ADD_IO:
          if(trackInput || trackOutput){
            entry->shadowEntries[i].operation = statePtrMapEntry->second;
            continue;
          }
          if(!trackCycles){
            entry->shadowEntries[i].operation = REDUNDANT;
          }
          continue;
        case STATE_ADD_CIO:
          if(trackCloneable || trackInput || trackOutput){
            entry->shadowEntries[i].operation = statePtrMapEntry->second;
            continue;
          }
          if(!trackCycles){
            entry->shadowEntries[i].operation = REDUNDANT;
          }
          continue;
        case STATE_ADD_TIO:
          if(trackTransfer || trackInput || trackOutput){
            entry->shadowEntries[i].operation = statePtrMapEntry->second;
            continue;
          }
          if(!trackCycles){
            entry->shadowEntries[i].operation = REDUNDANT;
          }
          continue;
        case STATE_ADD_OUTPUT:
          if(trackOutput){
            entry->shadowEntries[i].operation = statePtrMapEntry->second;
            continue;
          }
          if(!trackCycles){
            entry->shadowEntries[i].operation = REDUNDANT;
          }
          continue;
        case STATE_ADD_CO:
          if(trackCloneable || trackOutput){
            entry->shadowEntries[i].operation = statePtrMapEntry->second;
            continue;
          }
          if(!trackCycles){
            entry->shadowEntries[i].operation = REDUNDANT;
          }
          continue;
        case STATE_ADD_TO:
          if(trackTransfer || trackOutput){
            entry->shadowEntries[i].operation = statePtrMapEntry->second;
            continue;
          }
          if(!trackCycles){
            entry->shadowEntries[i].operation = REDUNDANT;
          }
          continue;
      }
    }
    else{
      continue;
    }

  }
  if(disableOptimizations){
    return;
  }
  oldState = batchState;
  entry->status = CLEANED_BATCH;
  //texasWorkEntriesQueue->push(entry);
  *progress = 2;
}

//This implements processShadowBatches(), but using a single thread only.
void singleShadowProcessShadowBatches(){
  bool kill = false;
  DEBUG("ProcessShadowBatches thread is alive!\n");
  shadowBatch* entry;

  while(1){
    //Check if there is new work to process
    if(texasWorkEntriesQueue->tryPop(entry)){
      if(entry->status == KILL_BATCH){
        DEBUG("Recieved kill command... wrapping things up now\n");
        return;
      }
      //Submit this batch to be cleaned
      else if(entry->status == NEW_BATCH){
        DEBUG("Recieved new batch... processing it\n");
        serialProcessShadowBatch(entry); 
      }
    }
  }
}
void processShadowBatches(){
  bool kill = false;
  DEBUG("ProcessShadowBatches thread is alive!\n");
  std::unordered_map<uint64_t, shadowBatch*> cleanedShadowBatches;
  std::unordered_map<uint64_t, shadowBatch*> newShadowBatches;
  shadowBatch* entry;
  std::thread* batchCleaner;
  volatile uint64_t batchBeingProcessed = 0;
  volatile uint64_t batchBeingCleaned = 0;
  //Initialize shadows
  Shadow** shadows = (Shadow**)calloc(NUM_SHADOWS, sizeof(Shadow*));
  for(auto i = 0; i < NUM_SHADOWS; i++){
    AllocationTable* lat = nullptr;
    Shadow* left  = nullptr;
    Shadow* right = nullptr;

    if(i == 0){
      lat = allocationMap;
    }
    else if(((i + 1) == NUM_SHADOWS) && NUM_SHADOWS != 1){
      lat = new AllocationTable(8);
    }
    else if(NUM_SHADOWS != 1){
      lat = new AllocationTable(8);
    }
    shadows[i] = new Shadow(i, lat, left, right);
  }
  //This will assign left and right shadows
  for(auto i = 0; i < NUM_SHADOWS; i++){
    if(i == 0 && NUM_SHADOWS != 1){
      shadows[i]->rightShadow = shadows[i+1];
    }
    else if(((i + 1) == NUM_SHADOWS) && NUM_SHADOWS != 1){
      shadows[i]->leftShadow = shadows[i-1];
    }
    else if(NUM_SHADOWS != 1){
      shadows[i]->leftShadow = shadows[i-1];
      shadows[i]->rightShadow = shadows[i+1];
    }
  }


  while(1){
    //Check if there is new work to process
    if(texasWorkEntriesQueue->tryPop(entry)){
      if(entry->status == KILL_BATCH){
        DEBUG("Recieved kill command... wrapping things up now\n");
        kill = true;
      }
      //Submit this batch to be cleaned
      else if(entry->status == NEW_BATCH){
        DEBUG("Recieved new batch... sending it into newShadowBatches\n");
        newShadowBatches.insert({entry->batchNumber, entry});
      }
    }

    //check if we can clean a batch
    if(batchBeingCleaned == 0){
      auto batchToClean = newShadowBatches.find(cleaningBatchNumber);
      if(batchToClean != newShadowBatches.end()){
        DEBUG("Cleaning new batch %lu\n", batchToClean->second->batchNumber);
        batchBeingCleaned = 1;
        batchCleaner = new std::thread(cleanBatch, batchToClean->second, &batchBeingCleaned);
      }
    }
    //check if we can clean a batch
    else if(batchBeingCleaned == 2){
      DEBUG("Finished cleaning batch %lu\n", cleaningBatchNumber);
      auto cleanedEntry = newShadowBatches.find(cleaningBatchNumber);
      cleanedShadowBatches.insert({cleaningBatchNumber, cleanedEntry->second});
      newShadowBatches.erase(cleaningBatchNumber);
      cleaningBatchNumber++;
      batchBeingCleaned = 0;

    }
    //Check if we can start a batch
    if(batchBeingProcessed == 0){
      auto batchToProcess = cleanedShadowBatches.find(servingBatchNumber);
      if(batchToProcess != cleanedShadowBatches.end()){
        if(servingBatchNumber != 0){
          cleanedShadowBatches.erase(servingBatchNumber - 1);
        }
        DEBUG("Processing cleaned batch %lu\n", batchToProcess->second->batchNumber);
        DEBUG("Num Entries:%lu\n", batchToProcess->second->numEntries);
        batchBeingProcessed = 1;
        for(auto i = 0; i < NUM_SHADOWS; i++){
          shadows[i]->thread = new std::thread(shadowRun, shadows[i], batchToProcess->second, &batchBeingProcessed);
        }
      }
    }
    //Clean up last batch processing
    else if(batchBeingProcessed == 2){
      DEBUG("Finished processing batch %lu, now joining threads back\n", servingBatchNumber);
      servingBatchNumber++;
      for(auto i = 0; i < NUM_SHADOWS; i++){
        shadows[i]->thread->join();
        shadows[i]->done = 0;
      }
      DEBUG("Finished joining threads back\n");
      batchBeingProcessed = 0;
    }
    if(kill && servingBatchNumber >= curBatchNumber && servingBatchNumber && !batchBeingProcessed && !batchBeingCleaned){
      DEBUG("Done running,, can finally die...\n");
      return;
    }

  }

}





//
//
//------------------INITIALIZATION CODE--------------------------------
//
//


class texasStartup{
  public:
    texasStartup(){

      texas_init();
    }
} __texasStartup;



void texas_init(){
  if (allocationMap == nullptr){
    if(const char* nt = std::getenv("CARMOT_THREADS")){
      NUM_SHADOWS = atoll(nt);
    }
    if(const char* ws = std::getenv("CARMOT_WINDOW_SIZE")){
      SHADOW_WINDOW = atoll(ws);
    }
#ifdef MEMORYTOOL_DISABLE_TEXAS_OPT
    disableOptimizations = 1;
    SHADOW_WINDOW = 1;
#endif

    //rsp = getrsp();
    allocationMap = new AllocationTable();
    allocationMap->allocConnections = new graph_t();
    texasWorkEntriesQueue = new texasWorkEntriesQueueType();
    originToAllocMap = new std::unordered_map<uint64_t, Allocation*>(); 

    cleanedTexasWorkEntriesQueue = new cleanedTexasWorkEntriesQueueType();
    uint64_t shadWin = SHADOW_WINDOW;

    //This will set up a limited amount of batches that can be produced at a given time.
    //NUM_BATCHES is found in Shadow.cpp/hpp
    texasReadyBatchEntriesQueue = new texasReadyBatchesQueueType(); 
    for(uint64_t i = 0; i < NUM_BATCHES; i++){
      texasReadyBatchEntriesQueue->push((shadowEntry*)malloc(shadWin * sizeof(shadowEntry)));
    }
    texasReadyBatchEntriesQueue->waitPop(texasWorkEntries);
    memset(texasWorkEntries, 0, shadWin);

    //Create an allocationEntry which will never have an allocation w/ higher address than it
    //This is to accomodate the tbb concurrent_map which does not allow us reverse iterators
    auto maxEntry = new Allocation((void*)0xffffffffffffffff, 0, 0);
    allocationMap->InsertAllocation((void*)0xffffffffffffffff, maxEntry);

    //Run the master background thread
    if(disableOptimizations){
      return;
    }
    shadowMasterThread = new std::thread(singleShadowProcessShadowBatches);


  }
  //printf("Leaving texas_init\n");
  return;
}

//
//
//--------------------------------FINISHING/REPORTING--------------------------------------
//
//

void ReportStatistics(){
  shadowBatch* newBatch = (shadowBatch*) malloc(sizeof(shadowBatch));
  newBatch->numEntries = slotStart ;
  newBatch->status = NEW_BATCH;
  newBatch->batchNumber = curBatchNumber;
  curBatchNumber++;
  newBatch->shadowEntries = texasWorkEntries;
  

  if(!disableOptimizations){
    texasWorkEntriesQueue->push(newBatch);
    //KILL shadows[0]
    newBatch = (shadowBatch*) malloc(sizeof(shadowBatch));
    newBatch->status = KILL_BATCH;
    texasWorkEntriesQueue->push(newBatch);
    shadowMasterThread->join();
  }
  else{
    //Process final batch
    serialProcessShadowBatch(newBatch);
  }

  DEBUG("Done joining batch processor thread, Now searching for cycles...\n");
  allocationMap->CommitStateInfoForAllocations();
  if(trackCycles){
    allocationMap->GenerateConnectionGraph();
    allocationMap->FindCyclesInConnectionGraph();
  }
  for(auto s : stateMap){
    s.second->CommitState();
  }
  ReportStates();

  //debugging time for mem consumption

  std::cout << "originToAllocMap size: " << originToAllocMap->size();


  inRT = true;
}

