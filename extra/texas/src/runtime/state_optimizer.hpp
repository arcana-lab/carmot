#include <stdint.h>
#include <stdlib.h>
#include <unordered_map>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

#include "state_tracking.hpp"
#include "texas.hpp"


extern "C"{
#include "state_packer.h"
}

#define ALIGNMENT_SIZE 8
#define CACHE_LINE_SIZE 64


typedef enum{
    TEMPORAL_HEAVIEST_EDGE_FIRST = 0,
    POINTER_HEAVIEST_EDGE_FIRST,
    TEMPORAL_HEAVIEST_NODE_FIRST,
    POINTER_HEAVIEST_NODE_FIRST,
}hueristic_t;

//This method will sort the nodes by starting with a node with the heaviest edge, then selecting the next node based off of which node has the highest weight with it.
void TemporalHeaviestEdgeFirst(state* state, std::unordered_set<allocEntry*>* unplacedAllocs, std::vector<allocEntry*>* packedAllocs);


//This function will optimize the given state
void OptimizeState(uint64_t stateID, hueristic_t hueristic);

//This function will fill a vector with the order to pack the given state
void GetOptimizedState(uint64_t stateID, hueristic_t hueristic, std::vector<allocEntry*>* packedAllocs);


//This will attempt to optimize every state
void optimizeStates();
