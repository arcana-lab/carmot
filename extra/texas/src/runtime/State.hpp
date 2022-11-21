#pragma once
#include "boostlibs.hpp"
#include "HelperFunctions.hpp"
#include <sstream>
#include <unordered_set>


class State;

extern std::unordered_map<uint64_t, State*> stateMap;

extern int stateReductionTracking;
extern int ComputationalSpoorTracking;

extern uint64_t GetAllocationSize(Allocation* alloc);

typedef tbb::concurrent_unordered_map<Allocation*, tbb::concurrent_unordered_set<uint64_t>*> stateAllocationsSetType;
typedef tbb::concurrent_unordered_set<void*> statePtrIterSetType;
typedef tbb::concurrent_unordered_set<uint64_t> stateInfoSetType;
typedef tbb::concurrent_unordered_map<Allocation*, stateInfoSetType*> stateAllocationInfoMapType;
typedef tbb::concurrent_unordered_map<Allocation*, uint64_t> temporalConnectionsEntryType;
typedef tbb::concurrent_unordered_map<Allocation*, temporalConnectionsEntryType*> temporalConnectionsType;

typedef std::unordered_map<void*, uint64_t> statePtrMapType;


class State{
  public:

    //These two things will point to the starting spot of state
    std::string functionName = "";
    uint64_t lineNumber = 0;

    uint64_t naiveStateSize = 0;
    uint64_t stateSize = 0;

    //These two things will point to the end spot for the state
    std::string endName = "";
    uint64_t endLineNum = 0;

    uint64_t stateID = 0;

    uint64_t temporalGraph = 0;
    Allocation* previousAlloc = nullptr;
    //Parent and child states
    State* parent = nullptr;
    std::set<State*> children;

    std::unordered_set<Allocation*> AllocsAllocatedInState;


    temporalConnectionsType temporalConnections;

    statePtrMapType statePtrMap;
    statePtrMapType statePtrIterMap;
    std::unordered_set<void*> currentPtrIterUpgrades;

    //This will map Allocations to their uses
    //This is a map from Allocation* to use info in source code
    //Example:
    //
    //main(){
    //a = malloc...
    //a[0] = 1; //COMPILER ASSIGNS UID 1
    //b = a[4]; //COMPILER ASSIGNS UID 2 and 3
    //}
    //
    //Allocation a -> {1, 2}
    //Allocation b -> {3}
    stateAllocationInfoMapType* allocationInfo ;

    //This will map Allocations within each set to the offsets associated with the membership
    
    //These map Allocation* -> {Offsets into the Allocation}
    //Example a = malloc(sizeof(uint64_t) * 10))
    //a[3] = 5 , this is in the output set
    //Allocation a -> {3}
    //
    //To get elements from the states:
    //
    //for(auto element : Input){
    //  Allocation* alloc = element.first
    //  tbb::concurrent_unordered_set<uint64_t>* offsets = element.second
    //}
    //
    //
    stateAllocationsSetType* Input;
    stateAllocationsSetType* IO;
    stateAllocationsSetType* CloneableIO;
    stateAllocationsSetType* TransferIO;

    stateAllocationsSetType* Output;
    stateAllocationsSetType* CloneableOutput;
    stateAllocationsSetType* TransferOutput;

    /*
     *The following sets are to be populated whenever a commit is seen
     *This will take the entries from the above sets and fill them into the below sets
     */
    stateAllocationsSetType* FinalInput;
    stateAllocationsSetType* FinalIO;
    stateAllocationsSetType* FinalCloneableIO;
    stateAllocationsSetType* FinalTransferIO;

    stateAllocationsSetType* FinalOutput;
    stateAllocationsSetType* FinalCloneableOutput;
    stateAllocationsSetType* FinalTransferOutput;





    //stats version of state creation
    State(char* funcName, uint64_t lineNum, uint64_t temporalTracking, uint64_t currentStateID, State* parentState);

    //default state creation
    State();
  
    void InitSpoorSets();


    //O(allocs * allocationMap * averageSize(allocToEscapeMap))
    //This algorithm looks at each Allocation and builds a mapping to every other Allocation
    //with a weight that gives how many escapes to itself are within those entries.

    //One potential issue with this is it currently only builds a mapping for allocs that are within the
    //state initially given. We could expand it to add allocs that have escapign references as well.
    void ProcessTemporalGraph();
    
    //This function will update the "final" sets with information from the currents sets.
    //It will then clear the sets
    void CommitState();
    void CommitAndCleanSet(stateAllocationsSetType*, stateAllocationsSetType*);

    //This will process the batch of state additions
    void processStateWindow();

    void CleanState(stateAllocationsSetType* setToDeleteFrom, stateAllocationsSetType* setToUseOffsetsFrom);

    void SetParentState(State*);
    uint64_t GetTransferStateMemoryFootprint(void);
    void ReportState(void);
    void GenerateTemporalGraph(void);
};

