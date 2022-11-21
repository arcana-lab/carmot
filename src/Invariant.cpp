#include "./include/Invariant.hpp"
#include "./include/LoadStoreInstrumentation.hpp"

std::unordered_set<Instruction*> getLoopInvariants(Module &M, Noelle &noelle, std::unordered_set<Instruction*> &instsToSkip, std::unordered_map<Instruction*, Entry*> &instToEntryMap){
  std::unordered_set<Instruction *> instsToInstrument;

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
        // If it isn't in the map, then it's not a load or a store, skip it
        if (instToEntryMap.count(&loopInstruction) == 0){
          continue;
        }

        if (instsToSkip.count(&loopInstruction)){
          continue;
        }

        //StoreInst *storeInst = dyn_cast<StoreInst>(&loopInstruction);
        //if (storeInst == nullptr){
        //  continue;
        //}

        Entry *entry = instToEntryMap[&loopInstruction];
        Value *addr = entry->allocPointer;

        // If the address the store instruction is storing values to
        // is loop invariant, then save that store instruction.
        // It will be instrumented only once.
        bool isLoopInvariant = loopInvariantManager->isLoopInvariant(addr);
        if (!isLoopInvariant){
          continue;
        }

        instsToInstrument.insert(&loopInstruction);
      }
    }
  }

  return instsToInstrument;
}

bool instrumentLoopInvariants(Module &M, std::map<Function *, Instruction *> &funcToCallstack, std::unordered_map<Instruction *, Entry *> &instToEntryMap, std::unordered_set<Instruction*> &instsToInstrument){
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
    auto storePointerType = addr->getType();
    auto parentFunc = I->getFunction();
    auto &firstBB = parentFunc->getEntryBlock();
    Instruction *insertPointAlloca = &(*(firstBB.getFirstInsertionPt()));
    //AllocaInst* newAllocation = new AllocaInst(storePointerType, 0, emptyName, insertPointAlloca);
    //AllocaInst* counterAllocation = new AllocaInst(int64Type, 0, emptyName, newAllocation);
    Constant *counterConstant0 = ConstantInt::get(int64Type, 0);
    GlobalVariable *counterAllocation = new GlobalVariable(M, int64Type, false, GlobalValue::CommonLinkage, counterConstant0);

    // Fill the new alloca with 0 the first time.
    //APInt value0(dataLayout.getPointerTypeSizeInBits(storePointerType), 0);
    //Constant *constant0 = Constant::getIntegerValue(storePointerType, value0);
    //StoreInst *allocInit = new StoreInst(constant0, newAllocation, false);
    //allocInit->insertAfter(newAllocation);

    //Constant *counterConstant0 = ConstantInt::get(int64Type, 0);
    //StoreInst *counterInit = new StoreInst(counterConstant0, counterAllocation, false);
    //counterInit->insertAfter(counterAllocation);

    // Split the store inst basic block in two.
    // The first instruction of the new basic block is the splitting instruction (i.e., store inst in our case).
    BasicBlock* oldBlock = I->getParent();
    BasicBlock* newBlock = oldBlock->splitBasicBlock(I, "");
    BasicBlock* wrapperBlock = BasicBlock::Create(context, emptyName, parentFunc, newBlock);

    // At the end of the old basic block compare the store pointer with alloca (containing the old store pointer value).
    // Remove the last instruction from oldBlock.
    oldBlock->getTerminator()->removeFromParent();
    // Create the comparison.
    Constant *constant2 = ConstantInt::get(int64Type, 2);
    LoadInst *load = new LoadInst(int64Type, counterAllocation, emptyName, oldBlock);
    ICmpInst *cmpInst = new ICmpInst(*oldBlock, ICmpInst::ICMP_ULT, load, constant2);
    // Based on compare, branch to the instrumentation (i.e., wrapper basic block), or to the store and rest of the code.
    BranchInst* brInst = BranchInst::Create(wrapperBlock, newBlock, cmpInst, oldBlock);

    // Populate the wrapper block.
    // Inject branch to newBlock.
    BranchInst* resumeRunning = BranchInst::Create(newBlock, wrapperBlock);

    // Add 1 to counter
    Constant *constant1 = ConstantInt::get(int64Type, 1);
    BinaryOperator *add1 = BinaryOperator::Create(Instruction::Add, load, constant1, emptyName, resumeRunning);
    // Store new counter value to counter
    StoreInst *counterUpdate = new StoreInst(add1, counterAllocation, false);
    counterUpdate->insertAfter(add1);
    
    // Get instrumentation for store.
    LoadStoreInstrumentation *loadStoreInstrumentation = getLoadStoreInstrumentation(M, entry, funcToCallstack);
    // Insert instrumentation in wrapper basic block.
    std::vector<Instruction *> instsToInsert = {loadStoreInstrumentation->pointerCast, loadStoreInstrumentation->addToState};
    Instruction *insertPoint = resumeRunning;
    insertInstructionsWithCheck(instsToInsert, insertPoint);

    // If we are here we modified the bitcode
    modified = true;
  }

  return modified;
}

