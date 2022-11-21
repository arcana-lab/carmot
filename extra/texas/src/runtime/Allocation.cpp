#include "Allocation.hpp"
#include <iostream>
uint64_t freshnessSetter = 1;

Allocation::Allocation(void* ptr, uint64_t len){
  pointer = ptr;
  length = len;
  allocToEscapeMap = new allocToEscapeMapType();
}

Allocation::Allocation(void* ptr, uint64_t len, uint64_t ori){
  pointer = ptr;
  length = len;
  allocToEscapeMap = new allocToEscapeMapType();
  origin = ori;
}

Allocation::~Allocation(){
  delete allocToEscapeMap;
}

uint64_t Allocation::getOrigin(void){
  return origin; 
}

void* Allocation::getVariablePointer(void){
  return pointer;
}

uint64_t Allocation::getVariableLength(void){
  return length;
}

allocToEscapeMapType* Allocation::getAllocationEscapes(void){
  return allocToEscapeMap;
}

bool Allocation::commitStateInfo(State* skipState){

  //Build a vector of skipState and its parents as well as affiliatedSTate and its parents. Eliminate any overlap between the two vectors.
  std::vector<State*> skipStateVec; 
  std::vector<State*> affilStateVec;
  State* tempState = skipState;
  while(tempState != nullptr){
    skipStateVec.push_back(tempState);
    tempState = tempState->parent;
  }
  tempState = this->affiliatedState;
  while(tempState != nullptr){
    affilStateVec.push_back(tempState);
    tempState = tempState->parent;
  }

  for(auto s1 : affilStateVec){
    for(auto s2 : skipStateVec){
      if(s1 == s2){
        this->StateCommitInfoMap.unsafe_erase(s2);
      }
    }
  }

  bool modified = false; 
  for(auto a : StateCommitInfoMap){
    modified = true;
    //For each new state, ensure we have a location for alloc info
    //State pointer
    auto State = a.first;
    //Set of use info (ints)
    auto Info = (a.second->useInfo);
    //Map of set membership along with associated offsets
    auto OffsetMap = (a.second->stateSetToOffsetMap);

    //Let us first add the allocation info
    auto InfoEntry = State->allocationInfo->find(this);
    if(InfoEntry == State->allocationInfo->end()){
      //First time populating this entry
      //Create a State Info Set Type
      stateInfoSetType* newUseSet = new stateInfoSetType();
      //Insert it into the allocationInfo
      State->allocationInfo->insert({this, newUseSet});
      InfoEntry = State->allocationInfo->find(this);
    }
    //Inject all the uses into the set
    for(auto use : *Info){
      InfoEntry->second->insert(use);
    }
    //Now that the uses are added, lets add membership to the sets
    for(auto set : *OffsetMap){
      //Determine path based on 
      stateAllocationsSetType* stateAllocSet = nullptr;
      switch(set.first){
        case STATE_ADD_INPUT:
          stateAllocSet = State->Input;
          break;
        case STATE_ADD_IO:
          stateAllocSet = State->IO;
          break;
        case STATE_ADD_CIO:
          stateAllocSet = State->CloneableIO;
          break;
        case STATE_ADD_TIO:
          stateAllocSet = State->TransferIO;
          break;
        case STATE_ADD_OUTPUT:
          stateAllocSet = State->Output;
          break;
        case STATE_ADD_CO:
          stateAllocSet = State->CloneableOutput;
          break;
        case STATE_ADD_TO:
          stateAllocSet = State->TransferOutput;
          break;
      }
      //Now we have the map, let us add the allocation offsets
      auto entry = stateAllocSet->find(this);
      if(entry == stateAllocSet->end()){
        //This is the first time this alloc is in the map, lets initialize things
        auto newSet = new tbb::concurrent_unordered_set<uint64_t>();
        //Insert it
        stateAllocSet->insert({this, newSet});
        entry = stateAllocSet->find(this);
      }
      //Put offsets in the set
      for(auto offset : *(set.second)){
        entry->second->insert(offset);
      }
    }
    //We committed the info, now we can clear/free things
  }
  //We can clean up the allocation now  
  for(auto a : StateCommitInfoMap){
    a.second->useInfo->clear();
    for(auto b : *(a.second->stateSetToOffsetMap)){
      b.second->clear();
    }
  }
  //StateCommitInfoMap.clear();
  return modified;
}

uint64_t GetAllocationSize(Allocation* alloc){
  return alloc->length;
}
