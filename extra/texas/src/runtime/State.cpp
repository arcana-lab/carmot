#include "State.hpp"
#include <unordered_set>

int stateReductionTracking = 1;
int ComputationalSpoorTracking = 1;


//Declare the stateMap
std::unordered_map<uint64_t, State*> stateMap;



void State::InitSpoorSets(){
  this->Input = new stateAllocationsSetType();
  this->IO = new stateAllocationsSetType();
  this->CloneableIO = new stateAllocationsSetType();
  this->TransferIO = new stateAllocationsSetType();
  this->Output = new stateAllocationsSetType();
  this->CloneableOutput = new stateAllocationsSetType();
  this->TransferOutput = new stateAllocationsSetType();

  this->FinalInput = new stateAllocationsSetType();
  this->FinalIO = new stateAllocationsSetType();
  this->FinalCloneableIO = new stateAllocationsSetType();
  this->FinalTransferIO = new stateAllocationsSetType();
  this->FinalOutput = new stateAllocationsSetType();
  this->FinalCloneableOutput = new stateAllocationsSetType();
  this->FinalTransferOutput = new stateAllocationsSetType();


}


State::State(char* funcName, uint64_t lineNum, uint64_t temporalTracking, uint64_t currentStateID, State* parentState){
  temporalGraph = temporalTracking;
  DEBUG("Temporal Tracking: %lu\n", temporalTracking);
  if(ComputationalSpoorTracking){
    InitSpoorSets();
  } 
  allocationInfo = new stateAllocationInfoMapType();
  functionName = funcName;
  stateID = currentStateID;
  lineNumber = lineNum;
  if(parentState){
    parent = parentState;
    parent->children.insert(this); 
  }
}

State::State(){
  temporalGraph = 0;
  if(ComputationalSpoorTracking){
    InitSpoorSets();  
  }
  allocationInfo = new stateAllocationInfoMapType();

}




void State::SetParentState(State* parent){
  this->parent = parent;
}

uint64_t State::GetTransferStateMemoryFootprint(void){
  this->stateSize = 0;
  std::unordered_set<Allocation*> transferAllocs;
  for(auto entry : *(this->TransferIO)){
    transferAllocs.insert(entry.first);
  }
  for(auto entry : *(this->TransferOutput)){
    transferAllocs.insert(entry.first);
  }
  for(auto entry : transferAllocs){
    this->stateSize += GetAllocationSize(entry); 
  }
  return this->stateSize;
}

void State::CleanState(stateAllocationsSetType* setToDeleteFrom, stateAllocationsSetType* setToUseOffsetsFrom){
  std::unordered_set<Allocation*> completeCleans;
  for(auto entry : *(setToUseOffsetsFrom)){
    auto allocSpecEntry = setToDeleteFrom->find(entry.first);
    if(allocSpecEntry != setToDeleteFrom->end()){
      for(auto offset : *(entry.second)){
        allocSpecEntry->second->unsafe_erase(offset);
      }
      if(!(allocSpecEntry->second->size())){
        completeCleans.insert(entry.first);
      }
    }
  }
  //This will remove the allocation completely from the setToDeleteFrom
  for(auto toErase : completeCleans){
    free(setToDeleteFrom->find(toErase)->second);
    setToDeleteFrom->unsafe_erase(toErase);
  }
}

void State::ReportState(){
  if(stateReductionTracking){
    printf("State %lu has a stateMemoryFootprint of %lu bytes (naively this would be %lu bytes)\n", this->stateID, this->GetTransferStateMemoryFootprint(), this->naiveStateSize);  
  }
  
  printf("The size of the state members:\n");
  printf("Input: %lu\n", this->Input->size());
  printf("Output: %lu\n", this->Output->size());
  printf("IO: %lu\n", this->IO->size());
  printf("CO: %lu\n", this->CloneableOutput->size());
  printf("CIO: %lu\n", this->CloneableIO->size());
  printf("TO: %lu\n", this->TransferOutput->size());
  printf("TIO: %lu\n\n", this->TransferIO->size());
  
  printf("The size of the FINAL state members:\n");
  printf("Input: %lu\n", this->FinalInput->size());
  printf("Output: %lu\n", this->FinalOutput->size());
  printf("IO: %lu\n", this->FinalIO->size());
  printf("CO: %lu\n", this->FinalCloneableOutput->size());
  printf("CIO: %lu\n", this->FinalCloneableIO->size());
  printf("TO: %lu\n", this->FinalTransferOutput->size());
  printf("TIO: %lu\n\n", this->FinalTransferIO->size());

  printf("State Allocation Member Info: %lu\n", this->allocationInfo->size());
  printf("Allocations Allocated in State: %lu\n", this->AllocsAllocatedInState.size());
  
  printf("StatePtrMap Size: %lu\n", this->statePtrMap.size());
  printf("StatePtrMapIter Size: %lu\n", this->statePtrIterMap.size());

  if(ComputationalSpoorTracking){
    //TIO can delete copies from CIO and IO 
    this->CleanState(this->CloneableIO, this->TransferIO);
    this->CleanState(this->IO, this->TransferIO);

    //CloneableIO can delete copies from IO
    this->CleanState(this->IO, this->CloneableIO);

    //IO can delete copies from Input
    this->CleanState(this->Input, this->IO);

    //TO can delete from CO and O
    this->CleanState(this->CloneableOutput, this->TransferOutput);
    this->CleanState(this->Output, this->TransferOutput);

    //Cloneable Output can delete from Output
    this->CleanState(this->Output, this->CloneableOutput);

  }

}

void State::CommitAndCleanSet(stateAllocationsSetType* tempSet, stateAllocationsSetType* finalSet){
  for(auto a : *(tempSet)){
    auto b = finalSet->find(a.first);
    if(b != finalSet->end()){
      for(auto c : *(a.second)){
        b->second->insert(c);
      }
      delete a.second;
    }
    else{
      finalSet->insert(a);
    }
  }
  tempSet->clear();
}


void State::CommitState(){
  this->CommitAndCleanSet(this->Input, this->FinalInput);
  this->CommitAndCleanSet(this->IO, this->FinalIO);
  this->CommitAndCleanSet(this->CloneableIO, this->FinalCloneableIO);
  this->CommitAndCleanSet(this->TransferIO, this->FinalTransferIO);
  this->CommitAndCleanSet(this->Output, this->FinalOutput);
  this->CommitAndCleanSet(this->CloneableOutput, this->FinalCloneableOutput);
  this->CommitAndCleanSet(this->TransferOutput, this->FinalTransferOutput);
  
  this->currentPtrIterUpgrades.clear();
  this->statePtrMap.clear();
  this->statePtrIterMap.clear();
}


void State::GenerateTemporalGraph(){

}
