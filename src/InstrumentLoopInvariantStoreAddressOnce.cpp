#include "./include/InstrumentLoopInvariantStoreAddressOnce.hpp"

void getLoopInvariantLoadStoreAddresses(Module &M, Noelle &noelle, std::unordered_set<Instruction*> &instsToSkip, std::unordered_map<Instruction*, Entry*> &instToEntryMap, std::unordered_set<Instruction*> &instrumentedInsts, std::unordered_map<LoadStoreInstInfo*, std::unordered_set<ExitBasicBlockInfo*>> &instsToInstrument){
  // Get program loops
  std::vector<LoopDependenceInfo*> *loops = noelle.getLoops();
  // For every loop
  for (LoopDependenceInfo *loop : *loops){
    InvariantManager *loopInvariantManager = loop->getInvariantManager();
    LoopStructure *loopStructure = loop->getLoopStructure();
    std::unordered_set<BasicBlock *> loopBasicBlocks = loopStructure->getBasicBlocks();
    // For every loop instruction
    for (BasicBlock *loopBasicBlock : loopBasicBlocks){
      for (Instruction &loopInstruction : *loopBasicBlock){
        // If it is in the map, then it's either a load or a store
        if (instToEntryMap.count(&loopInstruction)){

          if (instsToSkip.count(&loopInstruction)){
            continue;
          }

          StoreInst *storeInst = dyn_cast<StoreInst>(&loopInstruction);
          if (storeInst == nullptr){
            continue;
          }

          Entry *entry = instToEntryMap[&loopInstruction];

          Value *addr = entry->allocPointer;

          // If the address the store instruction is storing values to
          // is loop invariant, then save that store instruction.
          // It will be instrumented only once.
          bool isLoopInvariant = loopInvariantManager->isLoopInvariant(addr);
          if (!isLoopInvariant){
            continue;
          }

          getLoadStoreInstExitBasicBlock(noelle, &loopInstruction, loop, instsToInstrument);
          instrumentedInsts.insert(&loopInstruction);
        }
      }
    }
  }

  return;
}

