#include "wrapper.hpp"
#include <execinfo.h>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

#include "pin_interface.hpp"

#include "utils.hpp"


StatesToSave *statesToSave = nullptr;

//void FindCycleInAllocationMapWrapper(void);

std::unordered_set<void*> *loads = nullptr;
std::unordered_set<void*> *stores = nullptr;
uint64_t globalLoadCounter = 0;
uint64_t globalStoreCounter = 0;
uint64_t globalDirectStateCounter = 0;
uint64_t globalDirectStateSavedInstrumentation = 0;

bool disableTexas = (getenv("MEMORYTOOL_DISABLE_TEXAS") != NULL);

int disableStateUses = 0;
int disableCallstack = 0;

int traceCacheSize = 10;
int lruCacheSize = 200;
//int lruCacheSize = atoi(getenv("LRU_CACHE_SIZE"));
std::vector<Trace*> *traceCache = nullptr;
std::vector<Trace*> *uniqueTraces = nullptr;
std::unordered_map<size_t, std::vector<Trace*>> *hashToTraceMap = nullptr;
std::unordered_map<size_t, std::unordered_map<int, std::vector<DynamicTrace*>>> *traces = nullptr;
std::unordered_map<uint64_t, DynamicTrace*> *dynamicTraces;
uint64_t globalID = 1; // We cannot start from 0. ID 0 has a special meaning and it ignores uses.
std::vector<Trace*> tracesToDelete;

#ifdef MEMORYTOOL_STATISTICS
thread_local uint64_t globalNumberOfAccessesToTraces = 0;
thread_local uint64_t globalNumberOfDynamicIDCollisions = 0;
thread_local uint64_t globalNumberOfNptrsCollisions = 0;
thread_local uint64_t globalNumberOfNewTraceAccessesNoConflict = 0;
thread_local uint64_t globalNumberOfNewTraceAccesses = 0;
thread_local uint64_t globalNumberOfExistingTraceAccesses = 0;
thread_local uint64_t globalNumberOfCollisions = 0;
thread_local uint64_t traceLookupCollisions = 0;
thread_local uint64_t cacheHits = 0;
thread_local uint64_t cacheAccesses = 1;
thread_local uint64_t lruCacheHits = 0;
thread_local uint64_t totalBacktraceCalls = 1;
thread_local uint64_t lruCacheMisses = 0;

typedef std::chrono::high_resolution_clock Clock;
//thread_local double totalSaveCallStackTime = 0;
thread_local uint64_t totalSaveCallStackTime = 0;
thread_local uint64_t totalSaveCallStackCalls = 0;
thread_local uint64_t totalBacktraceTime = 0;
thread_local uint64_t totalBacktraceAsmTime = 0;
thread_local uint64_t totalHashPart1Time = 0;
thread_local uint64_t totalLookupTime = 0;
thread_local uint64_t totalFindTime = 0;
thread_local uint64_t totalLookupCacheTime = 0;
thread_local uint64_t totalSwapTime = 0;

thread_local uint64_t totalTimeLookupLRUCache = 0;
thread_local uint64_t totalTimeMissLRU = 0;
thread_local uint64_t totalTimeHitLRU = 0;
#endif

//Backtrace assembly
//extern "C" uint64_t backtrace_asm(void **trace, uint64_t max_depth, uint64_t temp1, uint64_t temp2);
extern "C" uint64_t backtrace_asm(long long *trace, uint64_t max_depth, uint64_t temp1, uint64_t temp2);

#ifdef MEMORYTOOL_STATISTICS
//RDTSC
//static inline uint64_t __attribute__((always_inline)) rdtsc (void){
//  uint32_t lo, hi;
//  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
//  return lo | ((uint64_t)(hi) << 32);
//}
#endif

class LRUCache{
  public:
    std::unordered_map<size_t, std::vector<Trace*>> hashToTraceMap;
    std::list<Trace*> dq;
    std::unordered_map<Trace*, std::list<Trace*>::iterator> ma;
    uint64_t csize;

    // Declare the size
    LRUCache(uint64_t csize):csize{csize}{ }

    // Refers key x with in the LRU cache
    Trace* refer(Trace &x){

#ifdef MEMORYTOOL_STATISTICS
      uint64_t time1 = rdtsc();
#endif

      // Check if the element is in the LRU cache
      Trace *traceInCache = nullptr;
      for (auto elem : this->dq){
        bool found = traceCompare(x, *elem);
        if (found){
          traceInCache = elem;
          break;
        }
      }

#ifdef MEMORYTOOL_STATISTICS
      uint64_t time2 = rdtsc();
      uint64_t time3 = 0;
      uint64_t time4 = 0;
#endif

      // not present in cache
      if (traceInCache == nullptr) {
#ifdef MEMORYTOOL_STATISTICS
        lruCacheMisses += 1;
#endif

        // cache is full
        if (dq.size() == csize) {
          // delete least recently used element
          Trace *last = dq.back();

          // Pops the last elmeent
          dq.pop_back();

          // Erase the last
          ma.erase(last);
        }

        // Find the element
        auto elem = this->hashToTraceMap.find(x.hashPart1);
        if (elem != this->hashToTraceMap.end()){ // found
          bool found = false;
          for (auto traceInMap : elem->second){
            bool isSameTrace = traceCompare(x, *traceInMap);
            if (!isSameTrace){
              continue;
            }

            traceInCache = traceInMap;
            found = true;
            break;
          }

          if (!found){
            traceInCache = new Trace(x);
            this->hashToTraceMap[traceInCache->hashPart1].push_back(traceInCache);
          }
        } else {
          traceInCache = new Trace(x);
          this->hashToTraceMap[traceInCache->hashPart1].push_back(traceInCache);
        }

        // Push it in lru cache
        dq.push_front(traceInCache);
        ma[traceInCache] = dq.begin();

#ifdef MEMORYTOOL_STATISTICS
        time3 = rdtsc();
#endif

      }
      // present in cache
      else {
#ifdef MEMORYTOOL_STATISTICS
        lruCacheHits += 1;
#endif
        // TODO: check if I am erasing and setting traceInCache to the same position in the list (check must be done in costant time) dq.begin() == ma[traceInCahce]
        auto traceInCacheIt = ma[traceInCache];
        if (dq.begin() != traceInCacheIt){
          dq.erase(traceInCacheIt);
          dq.push_front(traceInCache);
          ma[traceInCache] = dq.begin();
        }
#ifdef MEMORYTOOL_STATISTICS
        time4 = rdtsc();
#endif

        // TODO: implement disaplyCache() method
      }

#ifdef MEMORYTOOL_STATISTICS
      uint64_t timeLookupLRUCache = time2 - time1;
      totalTimeLookupLRUCache += timeLookupLRUCache;

      if (time3 != 0){
        uint64_t timeMissLRU = time3 - time2;
        totalTimeMissLRU += timeMissLRU;
      }

      if (time4 != 0){
        uint64_t timeHitLRU = time4 - time2;
        totalTimeHitLRU += timeHitLRU;
      }
#endif

      return traceInCache;
    }
};

LRUCache *lruCache = nullptr;

size_t myHashPart1(const Trace &trace){
  if (trace.nptrs <= 0){
    std::cerr << "Callstack is empty. Abort.\n";
    abort();
  }

  //std::hash<long long> pointerHash;
  //size_t accumulator = pointerHash(trace.buffer[0]);
  size_t accumulator = 0;
  for (auto i = 0; i < trace.nptrs; ++i){
    accumulator ^= ((size_t) trace.buffer[i]);
  }

  return accumulator;
}

size_t myHashPart2(size_t hashPart1, uint64_t staticID){
  std::hash<uint64_t> uint64Hash;
  size_t accumulator = hashPart1;
  accumulator ^= uint64Hash(staticID);

  return accumulator;
}

Trace* insertOrGetTrace(Trace &trace){
  if (traceCache == nullptr){
    traceCache = new std::vector<Trace*>();
  }

  Trace *traceRet = nullptr;

  // If cache is not full, just insert trace and return
  if (traceCache->size() < traceCacheSize){
    traceRet = new Trace(trace);
    traceCache->push_back(traceRet);
    return traceRet;
  }

  // If cache is full, then check if trace is there
  for (auto cachedTrace : *traceCache){
    bool found = traceCompare(trace, *cachedTrace);
    if (found){
      traceRet = cachedTrace;
      break;
    }
  }

  return traceRet;
}

// TODO: do not update every time, update every 10 000 000, reset accessFrequency to 0 (beneficial for phases?)
void swapLeastFrequentTrace(Trace *trace){
  Trace *tempTrace;
  for (auto cachedTrace : *traceCache){
    if (cachedTrace->accessFrequency < trace->accessFrequency){
      tempTrace = cachedTrace;
      cachedTrace = trace;
      trace = tempTrace;
      break;
    }
  }

  return;
}

void* getBacktraceUnique(uint64_t index, uint64_t numberOfUniqueTraces) {
  if (disableCallstack){
    return nullptr;
  }

  if (uniqueTraces == nullptr){
    uniqueTraces = new std::vector<Trace*>(numberOfUniqueTraces, nullptr);
  }

  Trace *trace = (*uniqueTraces)[index];
  if (trace != nullptr){
    return trace;
  }

  trace = new Trace{};

  uint64_t temp1, temp2;
  trace->nptrs = backtrace_asm(trace->buffer, TRACE_SIZE, temp1, temp2);
  trace->hashPart1 = myHashPart1(*trace);

  (*uniqueTraces)[index] = trace;

  return trace;
}

#ifdef MEMORYTOOL_LRUCACHE
void* getBacktrace(void) {
#ifdef MEMORYTOOL_STATISTICS
  totalBacktraceCalls += 1;
#endif

  if (disableCallstack){
    return nullptr;
  }

  Trace trace;
  Trace *traceRet = nullptr;

  uint64_t temp1, temp2;
  trace.nptrs = backtrace_asm(trace.buffer, TRACE_SIZE, temp1, temp2);

  // Compute first part of hash
  trace.hashPart1 = myHashPart1(trace);

  if (lruCache == nullptr){
    lruCache = new LRUCache(lruCacheSize);
  }
  traceRet = lruCache->refer(trace);

  return traceRet;
}
#endif

#ifdef MEMORYTOOL_NOCACHE
void* getBacktrace(void) {
#ifdef MEMORYTOOL_STATISTICS
  totalBacktraceCalls += 1;
#endif

  if (disableCallstack){
    return nullptr;
  }

  Trace trace;
  Trace *traceRet = nullptr;


  uint64_t temp1, temp2;
  trace.nptrs = backtrace_asm(trace.buffer, TRACE_SIZE, temp1, temp2);

  // Compute first part of hash
  trace.hashPart1 = myHashPart1(trace);

  // Check the map
  if (hashToTraceMap == nullptr){
    hashToTraceMap = new std::unordered_map<size_t, std::vector<Trace*>>();
  }

  auto elem = hashToTraceMap->find(trace.hashPart1);

  if (elem != hashToTraceMap->end()){ // found
    bool found = false;
    for (auto traceInMap : elem->second){
      bool isSameTrace = traceCompare(trace, *traceInMap);
      if (!isSameTrace){
        continue;
      }

      traceRet = traceInMap;
      found = true;
      break;
    }

    if (!found){
      traceRet = new Trace(trace);
      (*(hashToTraceMap))[trace.hashPart1].push_back(traceRet);
    }
  } else {
    traceRet = new Trace(trace);
    (*(hashToTraceMap))[trace.hashPart1].push_back(traceRet);
  }

  return traceRet;
}
#endif

#ifdef MEMORYTOOL_CACHE
void* getBacktrace(void) {
#ifdef MEMORYTOOL_STATISTICS
  totalBacktraceCalls += 1;
#endif

  if (disableCallstack){
    return nullptr;
  }

  Trace trace;
  Trace *traceRet = nullptr;

  uint64_t temp1, temp2;
  trace.nptrs = backtrace_asm(trace.buffer, TRACE_SIZE, temp1, temp2);

  // Compute first part of hash
  trace.hashPart1 = myHashPart1(trace);

  // TODO: put cache looup of most common (5?) backtrace here
  traceRet = insertOrGetTrace(trace);

  if (traceRet != nullptr){
    traceRet->accessFrequency += 1; // update access frequency
    return traceRet;
  }

  // Check the map
  if (hashToTraceMap == nullptr){
    hashToTraceMap = new std::unordered_map<size_t, std::vector<Trace*>>();
  }

  auto elem = hashToTraceMap->find(trace.hashPart1);
  if (elem != hashToTraceMap->end()){ // found
    bool found = false;
    for (auto traceInMap : elem->second){
      bool isSameTrace = traceCompare(trace, *traceInMap);
      if (!isSameTrace){
        continue;
      }

      //delete trace;
      traceRet = traceInMap;
      found = true;
      break;
    }

    if (!found){
      traceRet = new Trace(trace);
      (*(hashToTraceMap))[trace.hashPart1].push_back(traceRet);
    }
  } else {
    traceRet = new Trace(trace);
    (*(hashToTraceMap))[trace.hashPart1].push_back(traceRet);
  }

  // Check trace cache frequency, if current trace is more frequent than least frequent cached trace, then swap them
  traceRet->accessFrequency += 1; // update access frequency
  swapLeastFrequentTrace(traceRet);

  return traceRet;
}
#endif


void removeBacktrace(void){
  return;
  for (auto trace : tracesToDelete){
    delete trace;
  }
  tracesToDelete.clear();

  return;
}

#ifdef MEMORYTOOL_DISABLE_CLUSTERCALLSTACK
void* getBacktrace2(void) {
  Trace *trace = nullptr;

  if (disableCallstack){
    return trace;
  }

  // Assembly backtrace
  trace = new Trace();
  uint64_t temp1, temp2;
  trace->nptrs = backtrace_asm(trace->buffer, TRACE_SIZE, temp1, temp2);

  // Compute first part of hash
  trace->hashPart1 = myHashPart1(*trace);

  // Check the map
  if (hashToTraceMap == nullptr){
    hashToTraceMap = new std::unordered_map<size_t, std::vector<Trace*>>();
  }

  auto elem = hashToTraceMap->find(trace->hashPart1);
  if (elem != hashToTraceMap->end()){ // found 
    bool found = false;
    for (auto traceInMap : elem->second){
      if (trace->nptrs != traceInMap->nptrs){
        continue;
      }

      int res = memcmp(trace->buffer , traceInMap->buffer, trace->nptrs);
      if(res != 0){
        continue;
      }

      delete trace;
      trace = traceInMap;
      found = true;
      break;
    }

    if (!found){
      (*(hashToTraceMap))[trace->hashPart1].push_back(trace);
    }
  } else {
    (*(hashToTraceMap))[trace->hashPart1].push_back(trace);
  }

  return trace;
}
#endif

void prologue(int measureStateArg, int trackInputArg, int trackOutputArg, int trackCloneableArg, int trackTransferArg, int trackCyclesArg, int disableUsesArg, int disableCallstackArg){
  stateReductionTracking = measureStateArg;

  trackInput = trackInputArg;
  trackOutput = trackOutputArg;
  trackCloneable = trackCloneableArg;
  trackTransfer = trackTransferArg;
  trackCycles = trackCyclesArg;
  disableStateUses = disableUsesArg;
  disableCallstack = disableCallstackArg;

#ifdef MEMORYTOOL_DISABLE_TRACKING_OPTIONS
  trackInput = 1;
  trackOutput = 1;
  trackCloneable = 1;
  trackTransfer = 1;
  trackCycles = 1;
  disableStateUses = 0;
  disableCallstack = 0;
#endif

  return;
}

void caratStateRemoveFromAllocationTableWrapper(void *address) {
  if (disableTexas){
    return;
  }

#ifdef MEMORYTOOL_DEBUG
  std::cerr << "caratStateRemoveFromAllocationTableWrapper()\n";
#endif

#ifndef MEMORYTOOL_DISABLE_TEXAS
  RemoveFromAllocationTable(address);
#endif

  return;
}

uint64_t incrementPushReturn(Trace *trace, uint64_t staticID, size_t dynamicID){
  DynamicTrace *dynamicTrace = new DynamicTrace();
  dynamicTrace->trace = trace;
  dynamicTrace->staticID = staticID;
  dynamicTrace->uniqueID = globalID;
  globalID += 1;
  (*traces)[dynamicID][trace->nptrs].push_back(dynamicTrace);
  uint64_t uniqueID = dynamicTrace->uniqueID;
  (*dynamicTraces)[uniqueID] = dynamicTrace;

  return uniqueID;
}

uint64_t saveCallstack(uint64_t staticID, void *traceArg){
  if (traces == nullptr){
    traces = new std::unordered_map<size_t, std::unordered_map<int, std::vector<DynamicTrace*>>>{};
  }

  if (dynamicTraces == nullptr){
    dynamicTraces = new std::unordered_map<uint64_t, DynamicTrace*>{};
  }

  void *traceTmp = traceArg;
  uint64_t uniqueID = 0;

#ifdef MEMORYTOOL_DISABLE_CLUSTERCALLSTACK
  traceTmp = getBacktrace();
#endif

  if(traceTmp != nullptr){
    Trace *trace = static_cast<Trace*>(traceTmp);
    size_t dynamicID = myHashPart2(trace->hashPart1, staticID);
    if (traces->count(dynamicID) == 0){ // First trace with that ID
      uniqueID = incrementPushReturn(trace, staticID, dynamicID);

    } else {
      if ((*traces)[dynamicID].count(trace->nptrs)) { // Check if there are existing traces with same nptrs
        DynamicTrace *existingTrace = nullptr;
        for (auto elem : (*traces)[dynamicID][trace->nptrs]){
          // Compare hashPart1 of trace->buffer elem->buffer
          if (elem->trace->hashPart1 != trace->hashPart1){
            continue;
          }

          // If hashPart1 (static hashes) are the same, then check the whole callstack
          int res = memcmp(trace->buffer , elem->trace->buffer, trace->nptrs);
          if (res != 0){
            continue;
          }

          existingTrace = elem;
          //tracesToDelete.push_back(trace);

          break;
        }

        if (existingTrace == nullptr){
#ifdef MEMORYTOOL_STATISTICS
          // Check if there was at least 1 element in the vector
          //if ((*traces)[dynamicID][trace->nptrs].size() > 0){
          //  globalNumberOfCollisions += 1;
          //}
#endif

          uniqueID = incrementPushReturn(trace, staticID, dynamicID);

        } else {
          uniqueID = existingTrace->uniqueID;
        }

      } else { // There is at least a trace with the same dynamicID, but different nptrs
        uniqueID = incrementPushReturn(trace, staticID, dynamicID);
      }
    }

  }

  return uniqueID;
}

void caratStateAllocWrapper(uint64_t id, void *address, uint64_t length, void *traceArg) {
  if (disableTexas){
    return;
  }

#ifdef MEMORYTOOL_DEBUG
  std::cerr << "caratStateAllocWrapper()\n";
#endif

  uint64_t uniqueID = id;
  if (!disableCallstack){
    uniqueID = saveCallstack(id, traceArg);
  }

#ifndef MEMORYTOOL_DISABLE_TEXAS
  AddToAllocationTable(address, length, uniqueID);
#endif

  return;
}

void caratStateCallocWrapper(uint64_t id, void *address, uint64_t length,
    uint64_t sizeOfEntry, void *traceArg) {
  if (disableTexas){
    return;
  }

#ifdef MEMORYTOOL_DEBUG
  std::cerr << "caratStateCallocWrapper()\n";
#endif

  uint64_t uniqueID = id;
  if (!disableCallstack){
    uniqueID = saveCallstack(id, traceArg);
  }

#ifndef MEMORYTOOL_DISABLE_TEXAS
  AddCallocToAllocationTable(address, length, sizeOfEntry, uniqueID);
#endif

  return;
}

void caratStateReallocWrapper(uint64_t id, void *address, void *newAddress,
    uint64_t length, void *traceArg) {
  if (disableTexas){
    return;
  }

#ifdef MEMORYTOOL_DEBUG
  std::cerr << "caratStateReallocWrapper()\n";
#endif

  uint64_t uniqueID = id;
  if (!disableCallstack){
    uniqueID = saveCallstack(id, traceArg);
  }

#ifndef MEMORYTOOL_DISABLE_TEXAS
  HandleReallocInAllocationTable(address, newAddress, length, uniqueID);
#endif

  return;
}

void caratAddToEscapeTableWrapper(void *ptr){
  if (disableTexas){
    return;
  }

#ifdef MEMORYTOOL_DEBUG
  std::cerr << "caratAddToEscapeTableWrapper(), pointer: " << ptr << "\n";
#endif

#ifndef MEMORYTOOL_DISABLE_TEXAS
  AddToEscapeTable(ptr);
#endif

  return;
}

void texasAddToStateWithInfoWrapper(uint64_t id, void *ptr, uint64_t sizeInBytes, uint64_t sizeElementInBytes, void *traceArg) {

#ifdef MEMORYTOOL_INSTR_COUNTERS
  // We are outside of a ROI
  if (activeState == nullptr){
    return;
  }
  globalStoreCounter += 1;
#endif

  if (disableTexas){
    return;
  }

#ifdef MEMORYTOOL_DEBUG
  std::cerr << "texasAddToStateWithInfoWrapper()\n";
#endif

  // We are outside of a ROI
  if (activeState == nullptr){
    return;
  }

  // Check if we've seen this store in this dynamic invocation of the ROI
  if (stores == nullptr){
    stores = new std::unordered_set<void*>();
  }
  if (stores->count(ptr) != 0){
    return;
  }
  stores->insert(ptr);

  uint64_t uniqueID = id;
  if (disableStateUses){
    uniqueID = 0;
  } else {
    if (disableCallstack){
      uniqueID = id;
    } else {
      uniqueID = saveCallstack(id, traceArg);
    }
  }

#ifndef MEMORYTOOL_DISABLE_TEXAS
  char *ptrAsCharPtr = (char*) ptr;
  for (auto i = 0; i < sizeInBytes; i += sizeElementInBytes){
    AddToState(ptrAsCharPtr + i, 1, uniqueID);
  }
#endif

  return;
}

void texasAddToStateLoad(uint64_t id, void *ptr, void *traceArg) {

#ifdef MEMORYTOOL_INSTR_COUNTERS
  // We are outside of a ROI
  if (activeState == nullptr){
    return;
  }
  globalLoadCounter += 1;
#endif

  if (disableTexas){
    return;
  }

#ifdef MEMORYTOOL_DEBUG
  std::cerr << "texasAddToStateWithInfoWrapper()\n";
#endif

  // We are outside of a ROI
  if (activeState == nullptr){
    return;
  }

  // Check if we've seen this load in this dynamic invocation of the ROI
  if (loads == nullptr){
    loads = new std::unordered_set<void*>();
  }
  if (loads->count(ptr) != 0){
    return;
  }
  loads->insert(ptr);

  uint64_t uniqueID = id;
  if (disableStateUses){
    uniqueID = 0;
  } else {
    if (disableCallstack){
      uniqueID = id;
    } else {
      uniqueID = saveCallstack(id, traceArg);
    }
  }

#ifndef MEMORYTOOL_DISABLE_TEXAS
  AddToState(ptr, 0, uniqueID);
#endif

  return;
}

void setDirectState(uint64_t id, void *startPtr, int64_t startIndVar, int64_t stopIndVar, uint64_t elementBitwidth, uint64_t stateToSet, void *traceArg){

#ifdef MEMORYTOOL_INSTR_COUNTERS
  // We are outside of a ROI
  if (activeState == nullptr){
    return;
  }

  globalDirectStateCounter += 1;
  //globalDirectStateSavedInstrumentation += ((((stopIndVar - 1) - startIndVar)*elementBitwidth)/8)/4 ;
  globalDirectStateSavedInstrumentation += ((stopIndVar - 1) - startIndVar) ;
  std::cerr << "startIndVar = " << startIndVar << " stopIndVar = " << stopIndVar << " elementBitwidth = " << elementBitwidth << " FSAstate = " << stateToSet << "\n";
#endif

  if (disableTexas){
    return;
  }

#ifdef MEMORYTOOL_DEBUG
  std::cerr << "setDirectState() for allocation with ID " << id << " and FSA state " << stateToSet << "\n";
#endif

  // We are outside of a ROI
  if (activeState == nullptr){
    return;
  }

  uint64_t uniqueID = id;
  if (disableStateUses){
    uniqueID = 0;
  } else {
    if (disableCallstack){
      uniqueID = id;
    } else {
      uniqueID = saveCallstack(id, traceArg);
    }
  }

#ifndef MEMORYTOOL_DISABLE_TEXAS
  uint64_t lengthInBytes = (((stopIndVar - 1) - startIndVar)*elementBitwidth)/8;
  SetStateRegion(startPtr, lengthInBytes, activeState->stateID, stateToSet, uniqueID);
#endif

  return;
}

void texasAddMultiToStatePin(uint64_t id, void *traceArg){
  if (disableTexas){
    return;
  }

  // We are outside of a ROI
  //if (activeState == nullptr){
  //  return;
  //}

  uint64_t uniqueID = id;
  if (disableStateUses){
    uniqueID = 0;
  } else {
    if (disableCallstack){
      uniqueID = id;
    } else {
      uniqueID = saveCallstack(id, traceArg);
    }
  }

#ifndef MEMORYTOOL_DISABLE_TEXAS
  // Retrieve actions from PIN
  std::vector<Address*> addresses = retrieveAddressesVector();

  // Add addresses (malloc, free, load, store) to state
  for (auto elem : addresses){
    if (elem->type() == PIN_TOUCHED){
      // We are outside of a ROI
      if (activeState == nullptr){
        return;
      }

      TouchedAddress *touchedAddress = static_cast<TouchedAddress*>(elem);
      int isWritten = ((int) touchedAddress->isWritten);
      if (isWritten){
        // Check if we've seen this store in this dynamic invocation of the ROI
        if (stores == nullptr){
          stores = new std::unordered_set<void*>();
        }
        if (stores->count(touchedAddress->address) != 0){
          continue;
        }
        stores->insert(touchedAddress->address);
      } else {
        // Check if we've seen this load in this dynamic invocation of the ROI
        if (loads == nullptr){
          loads = new std::unordered_set<void*>();
        }
        if (loads->count(touchedAddress->address) != 0){
          continue;
        }
        loads->insert(touchedAddress->address);
      }

      AddToState(touchedAddress->address, isWritten, uniqueID);
      delete touchedAddress;

    } else if (elem->type() == PIN_MALLOC){
      MallocAddress *mallocAddress = static_cast<MallocAddress*>(elem);
      AddToAllocationTable(mallocAddress->address, mallocAddress->size, uniqueID);
      delete mallocAddress;

    } else if (elem->type() == PIN_FREE){
      FreeAddress *freeAddress = static_cast<FreeAddress*>(elem);
      RemoveFromAllocationTable(freeAddress->address);
      delete freeAddress;

    }

  }

#endif

  return;
}


#ifdef MEMORYTOOL_ROILATENCYCOVERAGE
class ROItime{
  public:
  uint64_t start;
  uint64_t total = 0;
};

std::unordered_map<uint64_t, ROItime> roiCoverageLatencyMap;
#endif

extern "C"
uint64_t caratGetStateWrapper(char *funcName, uint64_t lineNum) {
#ifdef MEMORYTOOL_INSTR_COUNTERS
  if (false){
#endif

  if (disableTexas){
#ifdef MEMORYTOOL_ROILATENCYCOVERAGE
    goto bypass;
#endif
    return 0;
  }

#ifdef MEMORYTOOL_INSTR_COUNTERS
  }
#endif

#ifdef MEMORYTOOL_ROILATENCYCOVERAGE
  bypass:
#endif

#ifdef MEMORYTOOL_DEBUG
  std::cerr << "caratGetStateWrapper()\n";
#endif

  uint64_t res = 0;

#ifndef MEMORYTOOL_DISABLE_TEXAS
  res = GetState(funcName, lineNum, 0);
#endif

#ifdef MEMORYTOOL_ROILATENCYCOVERAGE
  // Take ROI starting time
  roiCoverageLatencyMap[res].start = rdtsc();
#endif

  return res;
}

extern "C"
void caratReportStateWrapper(uint64_t stateID) {
#ifdef MEMORYTOOL_ROILATENCYCOVERAGE
  // Accumulate ROI time
  roiCoverageLatencyMap[stateID].total += (rdtsc() - roiCoverageLatencyMap[stateID].start);
#endif

#ifdef MEMORYTOOL_INSTR_COUNTERS
  if (false){
#endif

  if (disableTexas){
    return;
  }

#ifdef MEMORYTOOL_INSTR_COUNTERS
  }
#endif

#ifdef MEMORYTOOL_DEBUG
  std::cerr << "caratReportStateWrapper()\n";
#endif

  if (loads != nullptr){
    loads->clear();
  }

  if (stores != nullptr){
    stores->clear();
  }

#ifndef MEMORYTOOL_DISABLE_TEXAS
  EndState(stateID);
#endif

  return;
}

Trace* getTraceAndStaticID(uint64_t uniqueID, uint64_t &staticID){
  Trace *trace = nullptr;
  staticID = uniqueID;
  if (!disableCallstack){
    auto elem = dynamicTraces->find(uniqueID);
    if (elem == dynamicTraces->end()){
      std::cerr << "ERROR: uniqueID " << uniqueID << " not found. Abort.\n";
      abort();
    }
    DynamicTrace *dynamicTrace = elem->second;
    trace = dynamicTrace->trace;
    staticID = dynamicTrace->staticID;
  }

  return trace;
}

AllocationToSave getAllocationToSave(Allocation *alloc, stateInfoSetType* offsets, State *state){
  uint64_t uniqueID = alloc->origin;

  uint64_t staticID = 0; 
  Trace *trace = getTraceAndStaticID(uniqueID, staticID);

  AllocationToSave currAllocationToSave(trace, offsets, staticID, uniqueID, ((uint64_t) alloc->pointer), alloc->length, alloc->StateCommitInfoMap[state]->regionBasedOffsets);

  // Go through uses
  if (disableStateUses){
    return currAllocationToSave;
  }

  // Can this happen? Ask B$
  if (state == nullptr){
    return currAllocationToSave;
  }

  //if (alloc->StateCommitInfoMap.count(state) == 0){
  //  std::cerr << "ERROR: uses not found allocation " << alloc << ". Abort.\n";
  //  abort();
  //}

  //auto usesID = alloc->StateCommitInfoMap.at(state)->useInfo;
  auto usesID = state->allocationInfo->find(alloc);
  if (usesID == state->allocationInfo->end()){ // Allocatio NOT found
    std::cerr << "WARNING: not uses found for allocation.\n";
    return currAllocationToSave;
  }

  auto usesIDSet = usesID->second;
  //auto usesID = usesMap->at(alloc);
  for (auto useID : *usesIDSet){
    uint64_t useStaticID = 0;
    Trace *useTrace = getTraceAndStaticID(useID, useStaticID);
    currAllocationToSave.uses.push_back(UseToSave(useTrace, useStaticID, useID));
  }

  return currAllocationToSave;
}

void saveStateSet(stateAllocationsSetType *stateSet, State *state, StateSetToSave &stateSetToSave) {
  std::unordered_set<Allocation*> debugSetOfAllocations;
  for (auto elem : *(stateSet)){
    Allocation *alloc = elem.first;
    if (debugSetOfAllocations.count(alloc)){
      std::cerr << "Duplicate found of alloc, addr = " << alloc << "\n";
      continue;
    }
    debugSetOfAllocations.insert(alloc);
    stateInfoSetType* offsets = elem.second;
    AllocationToSave currAllocationToSave = getAllocationToSave(alloc, offsets, state);
    stateSetToSave.allocations.push_back(currAllocationToSave);
  }

  return;
}

void saveCycle(SmartPointerCycle *cycle, std::vector<CycleToSave> &cycles){
  cycles.push_back(CycleToSave{});
  CycleToSave &cycleToSave = cycles.back();
  for (auto allocation : cycle->cycle){
    AllocationToSave allocationToSave = getAllocationToSave(allocation, nullptr, nullptr);
    cycleToSave.cycle.push_back(allocationToSave);
  }
  cycleToSave.weakPoint = getAllocationToSave(cycle->weakPoint, nullptr, nullptr);
  //cycles.push_back(cycleToSave);

  return;
}

void saveCycles(AllocationTable *allocationMap, StatesToSave &statesToSave){
  // Iterate over cycles found at this point
  for (auto cycle: allocationMap->cycles){
    // Iterate over vector of state pointers this cycle belongs to
    for (auto cycleState : cycle->states){
      uint64_t stateID = cycleState->stateID;
      auto foundState = statesToSave.states.find(stateID);
      if (foundState == statesToSave.states.end()){
        // State not found
        statesToSave.states[stateID] = StateToSave();
        StateToSave &stateToSave = statesToSave.states[stateID];
        stateToSave.functionName = cycleState->functionName;
        stateToSave.lineNumber = cycleState->lineNumber;

        saveCycle(cycle, stateToSave.cycles);

      } else {
        // State found
        StateToSave &stateToSave = foundState->second;
        saveCycle(cycle, stateToSave.cycles);
      }
    }
  }

  return;
}

/*
void FindCycleInAllocationMapWrapper(uint64_t stateID){
  if (!trackCycles){
    return;
  }

#ifndef MEMORYTOOL_DISABLE_TEXAS
  volatile int syncValue = 0;
  FindCycleInAllocationMap(&syncValue, stateID);
  while (!syncValue){
    continue;
  }

  std::cerr << "SYNC VALUE = " << syncValue << "\n";

  // Now I have updated allocation map, save the cycles
  if (statesToSave == nullptr){
    statesToSave = new StatesToSave{};
  }
  saveCycles(allocationMap, *statesToSave);

  // Set it back to original value (free lock to TEXAS)
  syncValue = 0;
#endif

  return;
}
*/

extern "C"
void FindCycleInAllocationMapWrapper(void){
  if (!trackCycles){
    return;
  }

#ifndef MEMORYTOOL_DISABLE_TEXAS
  volatile int syncValue = 0;
  FindCycleInAllocationMap(&syncValue);
  while (!syncValue){
    continue;
  }

  syncValue = 0;
  return;

  std::cerr << "SYNC VALUE = " << syncValue << "\n";

  // Now I have updated allocation map, save the cycles
  if (statesToSave == nullptr){
    statesToSave = new StatesToSave{};
  }
  saveCycles(allocationMap, *statesToSave);

  // Set it back to original value (free lock to TEXAS)
  syncValue = 0;
#endif

  return;
}

void freeMemory(void){
  if (hashToTraceMap != nullptr){
    for (auto &elem : *hashToTraceMap){
      auto &traces = elem.second;
      for (auto trace : traces){
        delete trace;
      }
      traces.clear();
    }
    hashToTraceMap->clear();
    delete hashToTraceMap;
  }

  if (dynamicTraces != nullptr){
    for (auto &elem : *dynamicTraces){
      auto dynamicTrace = elem.second;
      delete dynamicTrace;
    }
    dynamicTraces->clear();
    delete dynamicTraces;
  }

  if (traces != nullptr){
    traces->clear();
    delete traces;
  }

  if (uniqueTraces != nullptr){
    for (auto elem : *uniqueTraces){
      delete elem;
    }
    uniqueTraces->clear();
    delete uniqueTraces;
  }

  if (loads != nullptr){
    loads->clear();
    delete loads;
  }

  if (stores != nullptr){
    stores->clear();
    delete stores;
  }

  if (statesToSave != nullptr){
    delete statesToSave;
  }

  return;
}

#ifdef MEMORYTOOL_STATISTICS
void copyCallstack(void *buffer[TRACE_SIZE], Trace *trace){
  for (auto i = 0; i < trace->nptrs; ++i){
    buffer[i] = (void*) trace->buffer[i];
  }

  return;
}
#endif

#ifdef MEMORYTOOL_STATISTICS
void printCallstack(Trace *trace){
  void *buffer[TRACE_SIZE];
  copyCallstack(buffer, trace);
  char **strings = backtrace_symbols(buffer, trace->nptrs);
  std::cerr << "callstack:\n";
  for (auto i = 0; i < trace->nptrs; ++i){
    std::cerr << strings[i] << "\n";
  }
  free(strings);

  return;
}
#endif

#ifdef MEMORYTOOL_STATISTICS
int64_t getNumelements(void){
  auto numOfElements = 0;
  for (auto &elem : *hashToTraceMap){
    numOfElements += elem.second.size();
    /*
       for (auto trace : elem.second){
       void *buffer[TRACE_SIZE];
       copyCallstack(buffer, trace);
       char **strings = backtrace_symbols(buffer, trace->nptrs);
       std::cerr << "callstack:\n";
       for (auto i = 0; i < trace->nptrs; ++i){
       std::cerr << strings[i] << "\n";
       }
       free(strings);
       }
       */
  }

  return numOfElements;
}
#endif

#ifdef MEMORYTOOL_STATISTICS
void printTraceVectorSizes(uint64_t limit){
  std::vector<std::pair<uint64_t, std::vector<Trace*>>> ranked;

  for (auto elem : *hashToTraceMap){
    ranked.push_back(std::make_pair(elem.second.size(), elem.second));
  }

  std::sort(ranked.begin(), ranked.end());

  std::cerr << "Highest vector sizes\n";
  if (limit == 0){
    limit = ranked.size();
  }
  auto j = 0;
  for(auto i = (ranked.size() - 1); i >= 0; --i){
    if (j >= limit){
      break;
    }
    j += 1;

    std::cerr << "size = " << ranked[i].first << "\n";
    for (auto elem : ranked[i].second){
      printCallstack(elem);
    }
    std::cerr << "\n";

    // Exit the loop, the rest of the values are 0
    if (ranked[i].first == 1){
      std::cerr << "The rest of sizes are 1. Stop printing.\n";
      break;
    }

  }

  return;
}
#endif

#ifdef MEMORYTOOL_STATISTICS
void printCollisionTraces(uint64_t limit){
  std::vector<std::pair<uint64_t, Trace*>> ranked;

  for (auto elem : *hashToTraceMap){
    for (auto trace : elem.second){
      ranked.push_back(std::make_pair(trace->collisions, trace));
    }
  }

  std::sort(ranked.begin(), ranked.end());

  std::cerr << "Highest collision traces\n";
  if (limit == 0){
    limit = ranked.size();
  }
  auto j = 0;
  for(auto i = (ranked.size() - 1); i >= 0; --i){
    if (j >= limit){
      break;
    }
    j += 1;

    std::cerr << "collisions = " << ranked[i].first << "\n";

    Trace *trace = ranked[i].second;
    printCallstack(trace);

    std::cerr << "\n";

    // Exit the loop, the rest of the values are 0
    if (ranked[i].first == 0){
      std::cerr << "The rest of collisions are 0. Stop printing.\n";
      break;
    }
  }

  return;
}
#endif

extern "C"
void endStateInvocationWrapper(uint64_t stateID){
  EndStateInvocation(stateID);
}

#ifdef MEMORYTOOL_ROILATENCYCOVERAGE
void printROICoverageLatencyMap(std::unordered_map<uint64_t, ROItime> &roiCoverageLatencyMap){
  for (auto elem : roiCoverageLatencyMap){
    auto currentState = stateMap[elem.first];
    std::cerr << "ROI " << currentState->functionName << " : " << currentState->lineNumber << " ";
    std::cerr << "CYCLES " << elem.second.total << "\n";
  }

  return;
}
#endif

void PrintJSONWrapper(void) {
#ifdef MEMORYTOOL_ROILATENCYCOVERAGE
  printROICoverageLatencyMap(roiCoverageLatencyMap);
#endif

#ifdef MEMORYTOOL_INSTR_COUNTERS
  std::cerr << "globalLoadCounter " << globalLoadCounter << "\n";
  std::cerr << "globalStoreCounter " << globalStoreCounter << "\n";
  std::cerr << "globalLoadStoreCounter " << globalLoadCounter + globalStoreCounter << "\n";
  std::cerr << "globalDirectStateCounter " << globalDirectStateCounter << "\n";
  std::cerr << "globalDirectStateSavedInstrumentationCounter " << globalDirectStateSavedInstrumentation << "\n";
#endif

  if (disableTexas){
    return;
  }

#ifndef MEMORYTOOL_DISABLE_TEXAS
  // Find cycles if trackCycles is true
  FindCycleInAllocationMapWrapper();
#endif

#ifdef MEMORYTOOL_BATCHING_OVERHEAD
  uint64_t curTime = rdtsc();
  auto iters = 100000000;
  //Ship 100,000,000 redundant operations
  for( auto i = 0; i < iters; i++){
    AddRedundantOp(); 
  }
  curTime = rdtsc() - curTime;
  std::cerr << "Batching takes " << curTime / iters <<  " cycles per push on average\n";
#endif

#ifndef MEMORYTOOL_DISABLE_TEXAS
  //THIS MUST BE CALLED
  ReportStatistics(); 
#endif

#ifdef MEMORYTOOL_DEBUG
  std::cerr << "PrintJSONWrapper()\n";
#endif

#ifndef MEMORYTOOL_DISABLE_TEXAS

  //StatesToSave statesToSave;
  if (statesToSave == nullptr){
    statesToSave = new StatesToSave{};
  }

  for(auto entry : stateMap){
    auto state = entry.second;

    uint64_t stateID = state->stateID;
    auto foundState = statesToSave->states.find(stateID);
    if (foundState == statesToSave->states.end()){
      // State not found, insert it
      statesToSave->states[stateID] = StateToSave();
    }

    // Retrieve state
    StateToSave &stateToSave = statesToSave->states[stateID];
    
    stateToSave.functionName = state->functionName;
    stateToSave.lineNumber = state->lineNumber;

    saveStateSet(state->FinalInput, state, stateToSave.I);
    saveStateSet(state->FinalOutput, state, stateToSave.O);
    saveStateSet(state->FinalIO, state, stateToSave.IO);
    saveStateSet(state->FinalCloneableOutput, state, stateToSave.CO);
    saveStateSet(state->FinalTransferOutput, state, stateToSave.TO);
    saveStateSet(state->FinalCloneableIO, state, stateToSave.CIO);
    saveStateSet(state->FinalTransferIO, state, stateToSave.TIO);
  }


  // Serialize
  // Create output archive
  std::ofstream ofs("runData.dat");
  boost::archive::binary_oarchive ar(ofs);

  // Save the data
  ar & *statesToSave;

#endif

#ifdef MEMORYTOOL_STATISTICS
  /*
     std::cerr << "globalNumberOfAccessesToTraces = " << globalNumberOfAccessesToTraces << std::endl;
     std::cerr << "globalNumberOfDynamicIDCollisions = " << globalNumberOfDynamicIDCollisions << std::endl;
     std::cerr << "globalNumberOfNptrsCollisions = " << globalNumberOfNptrsCollisions << std::endl;
     std::cerr << "globalNumberOfNewTraceAccessesNoConflict = " << globalNumberOfNewTraceAccessesNoConflict << std::endl;
     std::cerr << "globalNumberOfNewTraceAccesses = " << globalNumberOfNewTraceAccesses << std::endl;
     std::cerr << "globalNumberOfExistingTraceAccesses = " << globalNumberOfExistingTraceAccesses << std::endl;
     std::cerr << "globalNumberOfCollisions = " << globalNumberOfCollisions << std::endl;
     */
  //std::cerr << "Number of element in map = " << hashToTraceMap->size() << std::endl;
  //std::cerr << "Number of unique callstacks = " << getNumelements() << std::endl;
  //std::cerr << "Number of collisions in getBackTrace() = " << traceLookupCollisions << std::endl;
  //std::cerr << "Cache hit rate = " << ((double)cacheHits/(double)cacheAccesses) << std::endl;
  std::cerr << "LRU Cache hit rate = " << ((double)lruCacheHits/(double)totalBacktraceCalls) << std::endl;

  //printCollisionTraces(0);
  //printTraceVectorSizes(100);

  //std::cerr << "Average time (in cycles) from Trace to uniqueID (i.e., saveCallstack()) = " << ((double)totalSaveCallStackTime/(double)totalSaveCallStackCalls) << std::endl;
  //std::cerr << "Total saveCallstack() calls = " << totalSaveCallStackCalls << std::endl;

  //std::cerr << "Average time (in cycles) to execute getBacktrace() = " << ((double)totalBacktraceTime/(double)totalBacktraceCalls) << std::endl;
  //std::cerr << "Total getBacktrace() calls = " << totalBacktraceCalls << std::endl;

  //std::cerr << "Average time (in cycles) to execute backtrace_asm in getBacktrace() = " << ((double)totalBacktraceAsmTime/(double)totalBacktraceCalls) << std::endl;
  //std::cerr << "Average time (in cycles) to execute hashPart1 in getBacktrace() = " << ((double)totalHashPart1Time/(double)totalBacktraceCalls) << std::endl;
  //std::cerr << "Average time (in cycles) to execute map lookup in getBacktrace() = " << ((double)totalLookupTime/(double)totalBacktraceCalls) << std::endl;
  //std::cerr << "Average time (in cycles) to execute map find in getBacktrace() = " << ((double)totalFindTime/(double)totalBacktraceCalls) << std::endl;
  //std::cerr << "Average time (in cycles) to get or insert trace in cache in getBacktrace() = " << ((double)totalLookupCacheTime/(double)totalBacktraceCalls) << std::endl;
  //std::cerr << "Average time (in cycles) to swap trace in cache in getBacktrace() = " << ((double)totalSwapTime/(double)totalBacktraceCalls) << std::endl;

  std::cerr << "Average time (in cycles) to look up elements in LRU cache = " << ((double)totalTimeLookupLRUCache/(double)totalBacktraceCalls) << std::endl;
  std::cerr << "Average time (in cycles) hit LRU cache = " << ((double)totalTimeHitLRU/(double)lruCacheHits) << std::endl;
  std::cerr << "Average time (in cycles) miss LRU cache = " << ((double)totalTimeMissLRU/(double)lruCacheMisses) << std::endl;
#endif
  
  // Free memory
  freeMemory();

  return;
}

