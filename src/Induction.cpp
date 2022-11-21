#include "./include/Induction.hpp"

bool isConnectedToAnInductionVariableRecursive(Value *val, InductionVariableManager *inductionVariableManager, std::unordered_set<Value*> &visited){
  bool connectedToAnInductionVariable = false;
  if (visited.count(val)){
    return false;
  }
  visited.insert(val);

  Instruction *inst = dyn_cast<Instruction>(val);
  if (inst == nullptr){
    return false;
  }

  connectedToAnInductionVariable = inductionVariableManager->doesContributeToComputeAnInductionVariable(inst);
  if (connectedToAnInductionVariable){
    return true;
  }

  auto instOperandsNum = inst->getNumOperands();
  for (auto operandIndex = 0; operandIndex < instOperandsNum; ++operandIndex){
    connectedToAnInductionVariable = isConnectedToAnInductionVariableRecursive(inst->getOperand(operandIndex), inductionVariableManager, visited);
    if (connectedToAnInductionVariable){
      return true;
    }
  }

  return false;
}

bool isConnectedToAnInductionVariable(Value *val, InductionVariableManager *inductionVariableManager){
  std::unordered_set<Value*> visited;
  return isConnectedToAnInductionVariableRecursive(val, inductionVariableManager, visited);
}

bool checkSpecialInductionCondition(Value *addr, LoopStructure *loopStructure){
  PHINode *addrAsPhi = dyn_cast<PHINode>(addr);
  if (addrAsPhi == nullptr){
    return false;
  }

  auto included = 0;
  for(auto &incomingValue : addrAsPhi->incoming_values()) {
    Instruction *incomingValueAsInst = dyn_cast<Instruction>(incomingValue);
    if (incomingValueAsInst == nullptr){
      errs() << "WARNING: special induction variable check, PHINode incoming value " << *incomingValue << " not an instruction.\n";
      return false;
    }

    if (!loopStructure->isIncluded(incomingValueAsInst)){
      ++included;
      continue;
    }

    GetElementPtrInst *gepInst = dyn_cast<GetElementPtrInst>(incomingValueAsInst);
    if (gepInst == nullptr){
      return false;
    }

    if (!gepInst->hasAllConstantIndices()){
      return false;
    }
  }

  // If this is true, we always continued
  if (included == addrAsPhi->getNumIncomingValues()){
    return false;
  }

  return true;
}

std::unordered_set<Instruction*> getLoopInductions(Noelle &noelle, std::unordered_set<Instruction *> &instsToSkip, std::unordered_map<Instruction*, Entry*> &instToEntryMap) {
  std::unordered_set<Instruction *> instsToInstrument;

  // Get program loops
  std::vector<LoopDependenceInfo*> *loops = noelle.getLoops();
  // For every loop
  for (LoopDependenceInfo *loop : *loops){
    InductionVariableManager *loopInductionVariableManager = loop->getInductionVariableManager();
    InvariantManager *loopInvariantManager = loop->getInvariantManager();

    LoopStructure *loopStructure = loop->getLoopStructure();
    std::unordered_set<BasicBlock *> loopBasicBlocks = loopStructure->getBasicBlocks();
    // For every loop instruction
    for (BasicBlock *loopBasicBlock : loopBasicBlocks){
      for (Instruction &loopInstruction : *loopBasicBlock){
        // If it is in the map, then it's either a load or a store
        if (instToEntryMap.count(&loopInstruction) == 0){
          continue;
        }

        if (instsToSkip.count(&loopInstruction)) {
          continue;
        }

        Entry *entry = instToEntryMap[&loopInstruction];
        Value *addr = entry->allocPointer;

        // SPECIAL CASE
        bool isSpecialInductionVariable = checkSpecialInductionCondition(addr, loopStructure);
        if (isSpecialInductionVariable){
          instsToInstrument.insert(&loopInstruction);
          continue;
        }

        bool connectedToAnInductionVariable = isConnectedToAnInductionVariable(addr, loopInductionVariableManager);
        if (!connectedToAnInductionVariable){
          continue;
        }

        // Check if addr is a gep
        auto gepInst = dyn_cast<GetElementPtrInst>(addr);
        if (gepInst == nullptr) {
          continue;
        }

        instsToInstrument.insert(&loopInstruction);
      }
    }
  }

  return instsToInstrument;
}

