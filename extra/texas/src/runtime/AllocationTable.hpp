#pragma once
#include "boostlibs.hpp"
#include <cmath>
#include <iterator>
#include <unordered_set>
#include "Allocation.hpp"
#include "HelperFunctions.hpp"

typedef std::map<void*, Allocation*> AllocationTableType;
typedef std::unordered_map<void*, Allocation*> HashAllocationTableType;
typedef std::unordered_set<uint64_t> MasksType;

class SmartPointerCycle{
  public:
    std::vector<Allocation*> cycle;
    std::unordered_set<State*> states;
    Allocation* weakPoint;
};

class AllocationTable{
  public:

    //-----------------------VARIABLES-----------------------------
    //Sorted Map for NON-2^n allocations
    AllocationTableType* sortedAllocationTable;

    //Unsorted map for 2^n allocations compatible with masks
    HashAllocationTableType* hashAllocationTable;

    //This is a boost graph that can be used to show interconnectivity of allocations in the Allocation Table
    graph_t* allocConnections;

    //This is a set of all the masks to apply when searching for an allocation
    //pair.first is length being masked
    //pair.second is the mask itself
    MasksType* masks;
    MasksType* maskLengths;
    
    std::vector<SmartPointerCycle*> cycles;

    //------------------------CONSTRUCTORS------------------------- 
    //Default Constructor
    AllocationTable(void);

    //This constructor will add the integer to set of masks 
    AllocationTable(uint64_t mask);

    //This constructor will add the set of integers to become the masks
    AllocationTable(std::unordered_set<uint64_t> masks);



    //------------------------FUNCTIONS----------------------------

    //This function will search for an allocation given a pointer that potentially aliases an allocation
    Allocation* findAllocation(void* key);

    //This function will insert an allocation into the allocationTable
    void InsertAllocation(void* key, Allocation* entry);

    //This function will remove an allocation from the allocationTable
    void DeleteAllocation(void* key);

    //This function will clear all allocations in the allocationTable
    void Clear(void);

    //This function will add a mask to apply to keys when searching for an allocation for the hashmap
    void AddMaskForLength(uint64_t length);

    //This function will mask the pointer with the given mask
    void* MaskKey(void* ptr, uint64_t mask);


    //This function will merge one table into another
    //It will merge the hash allocation table and make a union of the masks
    //It will merge the normal allocation tables as normal
    //If bool is true, then AllocationTable* arg will overwrite entries in this
    void MergeTable(AllocationTable*, bool);

    //This function will go through all the allocations and commit their state information
    void CommitStateInfoForAllocations(void);

    //This function will return the size of the allocations in the allocation table (in bytes)
    uint64_t GetMemFootprint(void);

    //This function will produce a graph that shows the interconnectivity of the Allocations
    //Nodes: Allocations
    //Edges: Escape inside of one Allocation pointing to another Allocation
    void GenerateConnectionGraph(void);

    //This function will use the Connection Graph and attempt to find a cycle of references.
    //A cycle indicates that the allocations involved will not be able to be free'd by reference counting garbage collection
    void FindCyclesInConnectionGraph(void);
    void FindCyclesInConnectionGraph(uint64_t);
};

