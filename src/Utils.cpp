#include "./include/Utils.hpp"

StayConnectedNestedLoopForestNode* getROILoop(Noelle &noelle, CallBase *startTrackingCall){
  Function *startTrackingCaller = startTrackingCall->getFunction();
  errs() << "DEBUG: getROILoop() startTrackingCaller->getName() " << startTrackingCaller->getName() << "\n"; 
  std::vector<LoopStructure*> *functionLoops = noelle.getLoopStructures(startTrackingCaller);
  errs() << "DEBUG: after loop stractures, getROILoop() startTrackingCaller->getName() " << startTrackingCaller->getName() << "\n"; 
  StayConnectedNestedLoopForest *functionLoopsForest = noelle.organizeLoopsInTheirNestingForest(*functionLoops);
  StayConnectedNestedLoopForestNode *roiLoopForestNode = functionLoopsForest->getInnermostLoopThatContains(startTrackingCall);

  return roiLoopForestNode;
}

// Get a runtime function by name, abort if does not exist in the current
// module
Function *getRuntimeFunction(Module &M, std::string funcName) {
  Function *runtimeFunction = M.getFunction(funcName);

  if (runtimeFunction == nullptr) {
    errs() << "ERROR: function " << funcName
      << " does not exist in the current module. Abort.\n";
    abort();
  }

  return runtimeFunction;
}

// Insert instructions at a given entry point
void insertInstructions(std::vector<Instruction *> &insts, Instruction *insertPoint, bool insertBefore) {
  auto instsSize = insts.size();
  if (instsSize == 0) {
    return;
  }

  if (insertBefore){
    insts[0]->insertBefore(insertPoint);
  } else {
    insts[0]->insertAfter(insertPoint);
  }

  for (auto i = 1; i < instsSize; ++i) {
    insts[i]->insertAfter(insts[i - 1]);
  }

  return;
}

// Insert instructions at a given entry point checking for init functions and
// invoke
void insertInstructionsWithCheck(std::vector<Instruction *> &insts, Instruction *insertPoint) {
  bool insertBefore = false;
  Instruction *newInsertPoint = insertPoint;

  if (insertPoint->isTerminator()){
    insertBefore = true;
  }

  // If the insertion point is an invoke, then instrument only the normal
  // destination
  if (auto *invokeInst = dyn_cast<InvokeInst>(insertPoint)) {
    BasicBlock *normalDest = invokeInst->getNormalDest();
    newInsertPoint = &(*(normalDest->getFirstInsertionPt()));
  }

  // If we're here, then the insertPoint was not in an invoke, so it's a
  // normal instruction
  insertInstructions(insts, newInsertPoint, insertBefore);

  return;
}

// Try to get some valid DebugLoc
const DebugLoc &getDefaultDebugLoc(Value *value) {
  Instruction *inst = dyn_cast<Instruction>(value);
  // If it is not an instruction (i.e., it's a global), then give up and just return empy debug loc
  if (inst == nullptr){
    DebugLoc *newDebugLoc = new DebugLoc();
    return *newDebugLoc;
  }

  // Check if current instruction has debug loc, and return it
  const DebugLoc &instDebugLoc = inst->getDebugLoc();
  if (instDebugLoc) {
    return instDebugLoc;
  }

  // Otherwise, loop through instructions of current function, get the debug
  // loc info of first instruction that has them
  Function *F = inst->getFunction();
  for (auto &B : *F) {
    for (auto &I : B) {
      const DebugLoc &defaultDebugLoc = I.getDebugLoc();
      if (defaultDebugLoc) {
        return defaultDebugLoc;
      }
    }
  }

  // Finally, give up and just return empy debug loc
  DebugLoc *newDebugLoc = new DebugLoc();

  return *newDebugLoc;
}

bool ends_with(std::string const & value, std::string const & ending){
  if (ending.size() > value.size()){
    return false;
  }

  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}
