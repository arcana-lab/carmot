#include "./include/BaseAddressMostlyInvariant.hpp"

std::unordered_set<Instruction*> getBaseAddressMostlyInvariants(std::unordered_set<Instruction *> &instsToSkip, std::unordered_map<Instruction*, Entry*> &instToEntryMap) {
  std::unordered_set<Instruction *> instsToInstrument;

  // Go through memory modifying instructions (loads and stores)
  for (auto elem : instToEntryMap){
    Instruction *inst = elem.first;

    if (instsToSkip.count(inst)) {
      continue;
    }

    Entry *entry = elem.second;

    // Check if load/store address is a GEP
    Value *addr = entry->allocPointer;
    GetElementPtrInst *gepInst = dyn_cast<GetElementPtrInst>(addr);
    if (gepInst == nullptr){
      continue;
    }

    // Change the pointer we're tracking from the load/store address to the GEP base address
    entry->allocPointer = gepInst->getPointerOperand();

    // Insert load/store into set for instrumentation
    instsToInstrument.insert(inst);
  }

  return instsToInstrument;
}

