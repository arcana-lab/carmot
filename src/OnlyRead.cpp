#include "./include/OnlyRead.hpp"
#include "./include/LoadStoreInstrumentation.hpp"

std::unordered_set<Instruction*> getOnlyReadLoads(Noelle &noelle, StayConnectedNestedLoopForestNode *l, std::unordered_set<Instruction*> &instsToSkip, std::unordered_map<Instruction*, Entry*> &instToEntryMap){
  std::unordered_set<Instruction *> instsToInstrument;

  LoopDependenceInfo *ldi = noelle.getLoop(l);
  PDG *loopDG = ldi->getLoopDG();
  InvariantManager *loopInvariantManager = ldi->getInvariantManager();

  LoopStructure *ls = l->getLoop();
  std::unordered_set<BasicBlock *> loopBasicBlocks = ls->getBasicBlocks();
  
  uint64_t loadCounter = 0;
  uint64_t loadDFARemoved = 0;
  uint64_t storeCounter = 0;
  uint64_t storeDFARemoved = 0;
  // For every loop instruction
  for (BasicBlock *loopBasicBlock : loopBasicBlocks){
    for (Instruction &loopInstruction : *loopBasicBlock){
      // If it isn't in the map, then it's not a load or a store, skip it
      if (instToEntryMap.count(&loopInstruction) == 0){
        continue;
      }

      if (instsToSkip.count(&loopInstruction)){
        continue;
      }

      LoadInst *loadInst = dyn_cast<LoadInst>(&loopInstruction);
      if (loadInst == nullptr){
        continue;
      }


/*
      auto checkDependence = [](Value *fromValue, DGEdge<Value> *edge) -> bool {
        errs() << "EDGE " << *fromValue << "\n";
        return false;
      };

      loopDG->iterateOverDependencesTo(loadInst, false, true, false, checkDependence);
*/


      // Check that load is an invariant
      bool isLoopInvariant = loopInvariantManager->isLoopInvariant(loadInst);
      if (!isLoopInvariant){
        errs() << "DEBUG: load " << *loadInst << " not a loop invariant.\n";
        continue;
      }

      instsToInstrument.insert(loadInst);
    }
  }

  return instsToInstrument;
}


std::unordered_set<Instruction*> getOnlyReadLoadsAll(Noelle &noelle, std::vector<ROI*> &rois, std::unordered_set<Instruction*> &instsToSkip, std::unordered_map<Instruction*, Entry*> &instToEntryMap){
  std::unordered_set<Instruction*> instsToInstrument;

  for (ROI *roi : rois){
    CallBase *callToStartTracking = roi->start;
    StayConnectedNestedLoopForestNode *roiLoop = getROILoop(noelle, callToStartTracking);
    // Check if ROI is in a loop, if it is not, continue
    if (roiLoop == nullptr){
      continue;
    }

    std::unordered_set<Instruction*> instsToInstrumentTmp = getOnlyReadLoads(noelle, roiLoop, instsToSkip, instToEntryMap);
    std::set_union(instsToInstrumentTmp.begin(), instsToInstrumentTmp.end(), instsToInstrument.begin(), instsToInstrument.end(), std::inserter(instsToInstrument, instsToInstrument.begin()));
  }

  return instsToInstrument;
}

bool instrumentOnlyReads(Module &M, std::map<Function *, Instruction *> &funcToCallstack, std::unordered_map<Instruction *, Entry *> &instToEntryMap, std::unordered_set<Instruction*> &instsToInstrument){
  bool modified = false;

  // If there are no stores to instrument, just return.
  if (instsToInstrument.size() == 0) {
    return false;
  }

  // Get things we need.
  LLVMContext &context = M.getContext();
  Type *int64Type = Type::getInt64Ty(context);
  Twine emptyName = Twine("");
  DataLayout dataLayout(&M);

  for (auto I : instsToInstrument) {
    if (instToEntryMap.count(I) == 0){
      errs() << "ERROR: instruction " << *I << " does not have entry. Abort.\n";
      abort();
    }

    Entry *entry = instToEntryMap[I];
    Value *addr = entry->allocPointer;

    // Add a stack location for old value of pointer to first BB and counter as a global instead
    Type *type = addr->getType();
    if (!type->isPointerTy()) {
      errs() << "ERROR: this " << *type << " is not a pointer type. Abort.\n";
      abort();
    }

    PointerType *pointerType = dyn_cast<PointerType>(type);
    if (pointerType == nullptr){
      errs() << "ERROR: this pointer type " << *pointerType << " is not a pointer type. Abort.\n";
      abort();
    }

    auto parentFunc = I->getFunction();
    auto &firstBB = parentFunc->getEntryBlock();
    Instruction *insertPointAlloca = &(*(firstBB.getFirstInsertionPt()));

    ConstantPointerNull *nullptrConstant = ConstantPointerNull::get(pointerType);
    GlobalVariable *newAllocation = new GlobalVariable(M, type, false, GlobalValue::CommonLinkage, nullptrConstant);
    //Constant *counterConstant0 = ConstantInt::get(int64Type, 0);
    //GlobalVariable *counterAllocation = new GlobalVariable(M, int64Type, false, GlobalValue::CommonLinkage, counterConstant0);

    // Split the store inst basic block in two.
    // The first instruction of the new basic block is the splitting instruction (i.e., store inst in our case).
    BasicBlock* oldBlock = I->getParent();
    BasicBlock* newBlock = oldBlock->splitBasicBlock(I, "");
    BasicBlock* wrapperBlock = BasicBlock::Create(context, emptyName, parentFunc, newBlock);

    // At the end of the old basic block compare the store pointer with alloca (containing the old store pointer value).
    // Remove the last instruction from oldBlock.
    oldBlock->getTerminator()->removeFromParent();

    // Create the comparison.
    LoadInst *load = new LoadInst(pointerType, newAllocation, emptyName, oldBlock);
    ICmpInst *cmpInst = new ICmpInst(*oldBlock, ICmpInst::ICMP_EQ, addr, load);

    // Based on compare, branch to the instrumentation (i.e., wrapper basic block), or to the store and rest of the code.
    BranchInst* brInst = BranchInst::Create(newBlock, wrapperBlock, cmpInst, oldBlock);

    // Populate the wrapper block.
    // Inject branch to newBlock.
    BranchInst* resumeRunning = BranchInst::Create(newBlock, wrapperBlock);

    // Get instrumentation for store.
    LoadStoreInstrumentation *loadStoreInstrumentation = getLoadStoreInstrumentation(M, entry, funcToCallstack);
    // Insert instrumentation in wrapper basic block.
    std::vector<Instruction *> instsToInsert = {loadStoreInstrumentation->pointerCast, loadStoreInstrumentation->addToState};
    Instruction *insertPoint = resumeRunning;
    insertInstructionsWithCheck(instsToInsert, insertPoint);

    // Create and insert store inst to store current base address to global
    StoreInst *storeBaseAddrToGlobal = new StoreInst(addr, newAllocation, &(*wrapperBlock->getFirstInsertionPt()));

    // If we are here we modified the bitcode
    modified = true;
  }

  return modified;
}

