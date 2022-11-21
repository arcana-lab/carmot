#pragma once
#include "boostlibs.hpp"
#include "HelperFunctions.hpp"
#include "State.hpp"
#include <atomic>

typedef tbb::concurrent_unordered_set<uint64_t> useInfoType;
typedef tbb::concurrent_unordered_map<uint64_t, tbb::concurrent_unordered_set<uint64_t>*> stateSetToOffsetMapType;
typedef struct{
  useInfoType* useInfo;
  stateSetToOffsetMapType* stateSetToOffsetMap;
  bool regionBasedOffsets = false;
} StateCommitInfo;

extern uint64_t freshnessSetter;

typedef tbb::concurrent_unordered_set<void**> allocToEscapeMapType;
typedef tbb::concurrent_unordered_map<State*, StateCommitInfo*> StateCommitInfoMapType;

class Allocation{
  public:
    void* pointer = nullptr;
    uint64_t length = 0;
    allocToEscapeMapType* allocToEscapeMap;
    void* patchPointer = nullptr; 

    vertex_t connectionVertex;

    uint64_t totalPointerWeight = 0;
    uint64_t alignment = 8;

    uint64_t origin = 0;

    uint64_t freshness = 0;
    
    std::atomic_uint64_t occupied{0};
    State* affiliatedState = nullptr;

    //This will store all the states this allocation is a part of as well as use information
    StateCommitInfoMapType StateCommitInfoMap;
    Allocation() = delete;
    Allocation(void* ptr, uint64_t len, uint64_t origin);
    
    Allocation(void* ptr, uint64_t len);
    ~Allocation();

    //Accessor Functions
    allocToEscapeMapType* getAllocationEscapes(void);
    uint64_t getOrigin(void);
    std::string getVariableName(void);
    void* getVariablePointer(void);
    uint64_t getVariableLength(void);
    
    //returns true if info was committed
    bool commitStateInfo(State* skip);

};

uint64_t GetAllocationSize(Allocation* alloc);
