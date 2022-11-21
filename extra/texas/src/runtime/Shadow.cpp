#include "Shadow.hpp"
#include <unordered_map>
#include <unordered_set>

uint64_t NUM_SHADOWS = 4;
uint64_t NUM_BATCHES = 4;
uint64_t SHADOW_WINDOW = 524288; // GOOD
int trackCycles = 1;

std::unordered_map<uint64_t, Allocation*>* originToAllocMap;

//Constructor
Shadow::Shadow(int tid, AllocationTable* table, Shadow* left, Shadow* right){
  this->tid = tid;
  this->localAllocationTable = table;
  this->leftShadow = left;
  this->rightShadow = right;
  this->overflowWork = new texasWorkQueueType();
}

Shadow::~Shadow(){
  delete this->overflowWork;
}

void Shadow::processShadowBatch(shadowEntry* entry){
  this->overflowWork->clear();
  DEBUG("Tid %d bounds: Index %lu - %lu\n", this->tid, this->leftBound, this->rightBound);
  //Process our portion of the sub-batch
  for(uint64_t i = this->leftBound; i <= this->rightBound; i++){
    switch(entry[i].operation){
      case REDUNDANT:
        break;
      case ALLOC_ADD:
        this->processAddAlloc(&(entry[i]));
        break;
      case ALLOC_FREE:
        processFreeAllocation(&(entry[i]));
        break;
      case ESCAPE_ADD:
        processAddEscape(&(entry[i]));
        break;
      case STATE_ADD_READ:
      case STATE_ADD_WRITE:
        DEBUG("This isn't a valid control flow...\n");
      case STATE_ADD_INPUT:
      case STATE_ADD_IO:
      case STATE_ADD_CIO:
      case STATE_ADD_TIO:
      case STATE_ADD_OUTPUT:
      case STATE_ADD_CO:
      case STATE_ADD_TO:
        processAddToState(&(entry[i]));
        break;

      case STATE_COMMIT:
        //Cannot process unless tid 0
        if(this->leftShadow == nullptr){
          this->localAllocationTable->CommitStateInfoForAllocations();
          State* stateToCommit = entry[i].sizeState.statePtr;
          stateToCommit->CommitState();
        }
        else{
          this->leftShadow->overflowWork->push_back(&(entry[i]));
        }
        break;

      case STATE_REGION_SET:

        this->processStateRegionSet(&(entry[i]));

        break;

      case DETECT_CYCLE:
        //Cannot process unless tid 0
        if(this->leftShadow == nullptr){
          this->localAllocationTable->GenerateConnectionGraph();
          if(entry[i].sizeState.size == -1){            
            this->localAllocationTable->FindCyclesInConnectionGraph(); 
          }
          else{
            this->localAllocationTable->FindCyclesInConnectionGraph(entry[i].sizeState.size);
          }
          volatile int* syncVar = (volatile int*)entry[i].ptr;

          std::cerr << "SETTING SYNCVAR TO 1\n";
          *syncVar = 1;

          while(*syncVar){
            //SPIN BABY SPIN
          }
        }
        else{
          this->leftShadow->overflowWork->push_back(&(entry[i]));
        }
        break;
    }
  }

  //If we are the right most shadow, then we are done 
  if(this->rightShadow == nullptr){
    DEBUG("Tid %d done, Tid %d should be able to run now\n", this->tid, this->tid - 1);
    this->done = 1;
    return;
  }

  DEBUG("Tid %d is spinning on tid %d's done to finish\n", this->tid, this->rightShadow->tid);
  //Wait for thread to right of us to finish
  while(!(this->rightShadow->done)){
    //Spin
  }

  DEBUG("TID %d is merging to TID %d\n", this->tid, this->rightShadow->tid);
  //Now merge the rightShadow's allocation table into our own
  this->localAllocationTable->MergeTable(this->rightShadow->localAllocationTable, true);
  this->rightShadow->localAllocationTable->Clear();

  //Iterate through the overflow work of previous shadow
  for(shadowEntry* newEntry : *(this->rightShadow->overflowWork)){
    switch(newEntry->operation){
      case REDUNDANT:
        break;
      case ALLOC_ADD:
        this->processAddAlloc(newEntry);
        break;
      case ALLOC_FREE:
        processFreeAllocation(newEntry);
        break;
      case ESCAPE_ADD:
        processAddEscape(newEntry);
        break;
      case STATE_ADD_READ:
      case STATE_ADD_WRITE:
        DEBUG("This is a cycle related tracking\n");
      case STATE_ADD_INPUT:
      case STATE_ADD_IO:
      case STATE_ADD_CIO:
      case STATE_ADD_TIO:
      case STATE_ADD_OUTPUT:
      case STATE_ADD_CO:
      case STATE_ADD_TO:  
        processAddToState(newEntry);
        break;
      case STATE_COMMIT:
        //Cannot process unless tid 0
        if(this->leftShadow == nullptr){
          this->localAllocationTable->CommitStateInfoForAllocations();
          State* stateToCommit = newEntry->sizeState.statePtr;
          stateToCommit->CommitState();
        }
        else{
          this->leftShadow->overflowWork->push_back(newEntry);
        }
        break;
      case STATE_REGION_SET:
        this->processStateRegionSet(newEntry);
        break;
      case DETECT_CYCLE:
        //Cannot process unless tid 0
        if(this->leftShadow == nullptr){
          this->localAllocationTable->GenerateConnectionGraph();
          if(newEntry->sizeState.size == -1){            
            this->localAllocationTable->FindCyclesInConnectionGraph(); 
          }
          else{
            this->localAllocationTable->FindCyclesInConnectionGraph(newEntry->sizeState.size);
          }
          volatile int* syncVar = (volatile int*)newEntry->ptr;
          *syncVar = 1;
          while(*syncVar){
            //SPIN BABY SPIN
          }
        }
        else{
          this->leftShadow->overflowWork->push_back(newEntry);
        }
        break;
    }
  }


  this->done = 1;
  return;

}



//This function can be seen as a bypass to setting states with reads and writes
void Shadow::processStateRegionSet(shadowEntry* shadEntry){
  void* allocPtr = shadEntry->ptr;
  //prospective will be either not found at all, matches addressEscaping exactly, or is next block after addressEscaping      
  Allocation* entry = this->localAllocationTable->findAllocation(allocPtr);

  //If we do not find the allocation, maybe it is in a different allocationMap?
  if(entry == nullptr){
    if(this->leftShadow != nullptr){
      this->overflowWork->push_back(shadEntry);
    }
    return;
  }

  //Make sure there is a state entry in the Allocation
  if(!entry->StateCommitInfoMap.count(shadEntry->affiliatedState)){
    StateCommitInfo* newStateCommitInfoEntry = (StateCommitInfo*)malloc(sizeof(StateCommitInfo));
    newStateCommitInfoEntry->stateSetToOffsetMap = new stateSetToOffsetMapType();
    newStateCommitInfoEntry->useInfo = new useInfoType();
    entry->StateCommitInfoMap.insert({shadEntry->affiliatedState, newStateCommitInfoEntry});
  }
  auto stateInfo = entry->StateCommitInfoMap[shadEntry->affiliatedState];
  //Add use info
  stateInfo->useInfo->insert(shadEntry->information);

  //Ensure there is a stateOffsetEntry

  if(!stateInfo->stateSetToOffsetMap->count(shadEntry->stateToSet)){
    stateInfo->stateSetToOffsetMap->insert({shadEntry->stateToSet, new tbb::concurrent_unordered_set<uint64_t>()}); 
  }
  auto offsetSet = stateInfo->stateSetToOffsetMap->at(shadEntry->stateToSet);

  //Let's set the state in the allocation
  uint64_t beginningOffset = GetOffset(entry->pointer, shadEntry->ptr); 
  uint64_t endOffset = beginningOffset + shadEntry->sizeState.size;
  
  stateInfo->regionBasedOffsets = true;
  offsetSet->insert(beginningOffset);
  offsetSet->insert(endOffset);

  //UPDATED: Only insert the beginning and end offsets when using Region set
  //for(auto i = beginningOffset; i < endOffset; i += 4){
   // offsetSet->insert(i);
 // }
  
  //Now that the state is updated, let us update the state knowledge of this
  //UPDATED: No longer do this as state is guarenteed when set.
  //auto affState = shadEntry->affiliatedState;
  //for(auto i = beginningOffset; i < endOffset; i += 4){
  //  affState->statePtrMap.insert_or_assign((void*)(((uint64_t)entry->pointer) + i), shadEntry->stateToSet);
  //}

  State* parentState = shadEntry->affiliatedState->parent;
  if(parentState != nullptr){
    DEBUG("Recursing????\n");
    shadEntry->affiliatedState = parentState;
    processStateRegionSet(shadEntry);
  }

}


void Shadow::processAddAlloc(shadowEntry* entry){
  Allocation* newEntry = nullptr; 
  auto potentialAllocEntry = originToAllocMap->find(entry->information);
  if(potentialAllocEntry == originToAllocMap->end()){
    newEntry = new Allocation(entry->ptr, entry->sizeState.size, entry->information);
    originToAllocMap->insert(std::make_pair(entry->information, newEntry));
  }
  else{
    newEntry = potentialAllocEntry->second;
    newEntry->pointer = entry->ptr;
    newEntry->length = entry->sizeState.size;
  } 
  this->localAllocationTable->InsertAllocation(entry->ptr, newEntry);
  newEntry->affiliatedState = entry->affiliatedState;
  if(newEntry->affiliatedState != nullptr){
    newEntry->affiliatedState->AllocsAllocatedInState.insert(newEntry);
    State* par = newEntry->affiliatedState->parent;
    while(par != nullptr){
      par->AllocsAllocatedInState.insert(newEntry);
      par = par->parent;
    }
  }
}

void Shadow::processAddEscape(shadowEntry* entry){
  void** addressEscaping = (void**)entry->ptr;

  if(addressEscaping == nullptr || *addressEscaping == nullptr){
    return;
  }
  auto* alloc = this->localAllocationTable->findAllocation(*addressEscaping);
  if(alloc == nullptr){
    if(this->leftShadow != nullptr){
      this->overflowWork->push_back(entry);
    }
    return;
  }
  alloc->allocToEscapeMap->insert(addressEscaping);  


}

void Shadow::processAddToState(shadowEntry* shadEntry){
  void* allocPtr = shadEntry->ptr;
  //prospective will be either not found at all, matches addressEscaping exactly, or is next block after addressEscaping      


  Allocation* entry = this->localAllocationTable->findAllocation(allocPtr);


  if(entry == nullptr){
    if(this->leftShadow != nullptr){
      this->overflowWork->push_back(shadEntry);
    }
    return;
  }

  if(trackCycles){
    entry->freshness = freshnessSetter;
    freshnessSetter++;
    if(shadEntry->operation == STATE_ADD_WRITE || shadEntry->operation == STATE_ADD_READ){
      return;
    }
  }


  auto statesEntry = entry->StateCommitInfoMap.find(shadEntry->sizeState.statePtr);
  //Ensure statesEntry is valid
  if(statesEntry == entry->StateCommitInfoMap.end()){
    //Create the StateCommitInfoMap
    StateCommitInfo* newStateCommitInfoEntry = (StateCommitInfo*)malloc(sizeof(StateCommitInfo));
    newStateCommitInfoEntry->useInfo = new useInfoType();
    newStateCommitInfoEntry->stateSetToOffsetMap = new stateSetToOffsetMapType();
    entry->StateCommitInfoMap.insert({shadEntry->sizeState.statePtr, newStateCommitInfoEntry});
    statesEntry = entry->StateCommitInfoMap.find(shadEntry->sizeState.statePtr);
    statesEntry->second->regionBasedOffsets = false;
  }
  if(shadEntry->information){
    statesEntry->second->useInfo->insert(shadEntry->information);
  }
  auto offsetEntry = statesEntry->second->stateSetToOffsetMap->find(shadEntry->operation);
  if(offsetEntry == statesEntry->second->stateSetToOffsetMap->end()){
    //Create the set for this state
    auto newSet = new tbb::concurrent_unordered_set<uint64_t>();
    statesEntry->second->stateSetToOffsetMap->insert({shadEntry->operation, newSet});
    offsetEntry = statesEntry->second->stateSetToOffsetMap->find(shadEntry->operation);
  }
  //This will give us the byte offset into the alloc that this ptr is
  uint64_t offset = ( ( ((uint64_t)shadEntry->ptr) - ((uint64_t)entry->pointer)  ) );

  offsetEntry->second->insert(offset);


  State* parentState = shadEntry->sizeState.statePtr->parent;
  if(parentState != nullptr){
    DEBUG("Recursing????\n");
    shadEntry->sizeState.statePtr = parentState;
    processAddToState(shadEntry);
  }
}

void Shadow::processFreeAllocation(shadowEntry* entry){
  Allocation* alEntry = this->localAllocationTable->findAllocation(entry->ptr);
  if(alEntry == nullptr){
    //Need to push it to the left shadow
    if(this->leftShadow != nullptr){
      this->overflowWork->push_back(entry);
    }
    return;
  }
  alEntry->commitStateInfo(entry->affiliatedState);
  this->localAllocationTable->DeleteAllocation(entry->ptr);

}

void Shadow::processStateBeginEnd(shadowEntry* entry){
  //Add state pointer to state candidates
}
