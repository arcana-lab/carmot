#include "./include/StoreLoopInstrumentation.hpp"

void getLoadStoreInstExitBasicBlock(Noelle &noelle, Instruction *inst, LoopDependenceInfo *loop, std::unordered_map<LoadStoreInstInfo *, std::unordered_set<ExitBasicBlockInfo *>> &loadsStoresToInstrument) {
  Function *storeInstFunc = inst->getFunction();

  // TODO (for compile-time performance, low prio): check if a new dominatorSummary is returned every time, of these are pre-computed.
  // If a new object is returned every time, then cluster store inst by function, so this can be called only once.
  DominatorSummary *dominatorSummary = noelle.getDominators(storeInstFunc);
  DomTreeSummary &DT = dominatorSummary->DT;

  // For every loop exit basic block insert check for alloca inst to be 1
  // and TEXAS instrumentation.
  BasicBlock *storeBB = inst->getParent();
  LoopStructure *loopStructure = loop->getLoopStructure();
  BasicBlock *storeLoopPreHeader = loopStructure->getPreHeader();
  std::vector<std::pair<BasicBlock*, BasicBlock*>> loopExitEdges = loopStructure->getLoopExitEdges();

  LoadStoreInstInfo *loadStoreInstInfo =
      new LoadStoreInstInfo(inst, storeLoopPreHeader);
  for (auto &elem : loopExitEdges){
    BasicBlock *exitingBasicBlock = elem.first;
    BasicBlock *exitBasicBlock = elem.second;
    bool dominates = DT.dominates(storeBB, exitBasicBlock);
    ExitBasicBlockInfo *exitBasicBlockInfo = new ExitBasicBlockInfo(exitingBasicBlock, exitBasicBlock, dominates);
    loadsStoresToInstrument[loadStoreInstInfo].insert(exitBasicBlockInfo);
  }

  return;
}

void instrumentLoopLoadStoreAddress(Module &M, std::unordered_map<LoadStoreInstInfo *, std::unordered_set<ExitBasicBlockInfo *>> &instsToInstrument, std::map<Function *, Instruction *> &funcToCallstack, std::unordered_map<Instruction *, Entry *> &instToEntryMap, std::unordered_map<BasicBlock *, std::vector<CheckInstrumentBasicBlock *>> &exitBBToChainMap, std::unordered_map<BasicBlock *, BasicBlock *> &exitBBToExitingBBMap) {
  // Get things we need.
  LLVMContext &context = M.getContext();
  Twine emptyName = Twine("");
  DataLayout dataLayout(&M);

  for (auto elem : instsToInstrument) {
    LoadStoreInstInfo *storeInstInfo = elem.first;
    Instruction *inst = storeInstInfo->inst;

    // For every loop exit basic block insert check for alloca inst to be != 0
    // and TEXAS instrumentation.
    if (instToEntryMap.count(inst) == 0) {
      errs() << "Corresponding entry for store inst " << *inst << " not found. Abort.";
      abort();
    }

    Entry *entry = instToEntryMap[inst];

    Value *addr = entry->allocPointer;
    BasicBlock *loopPreHeaderBasicBlock = storeInstInfo->loopPreHeaderBasicBlock;
    auto storePointerType = addr->getType();
    Function *storeInstFunc = inst->getFunction();

    // Create new alloca inst for store address (set to store address is executed),
    // insert it at the beginning of the entry basic block of the function the store is.
    Instruction *insertPointAlloca = &(*(storeInstFunc->getEntryBlock().getFirstInsertionPt()));
    AllocaInst *allocaInst =
      new AllocaInst(storePointerType, 0, emptyName, insertPointAlloca);
    // Set it to 0.
    APInt value0(dataLayout.getPointerTypeSizeInBits(storePointerType), 0);
    Constant *constant0 = Constant::getIntegerValue(storePointerType, value0);
    //Constant *constant0 = ConstantInt::get(storePointerType, 0);
    StoreInst *store0 = new StoreInst(constant0, allocaInst, loopPreHeaderBasicBlock->getTerminator());

    // Create extra store instruction that stores the store address into newly created alloca inst,
    // insert it at the end of the store basic block.
    StoreInst *store1 = new StoreInst(addr, allocaInst);
    store1->insertAfter(inst);

    for (auto exitBasicBlockInfo : elem.second){
      Entry *entryCopy = entry->copy();

      BasicBlock *exitingBasicBlock = exitBasicBlockInfo->exitingBasicBlock;
      BasicBlock *exitBasicBlock = exitBasicBlockInfo->exitBasicBlock;
      if (exitBBToExitingBBMap.count(exitBasicBlock) == 0){
        exitBBToExitingBBMap[exitBasicBlock] = exitingBasicBlock;
      }
      // if exit BB already in the map check that exiting BB is the same
      if (exitBBToExitingBBMap[exitBasicBlock] != exitingBasicBlock){
        //errs() << "MODULE\n" << M << "\n";
        errs() << "INST BB " << *(inst->getParent()) << "\n";
        errs() << "ERROR: trying to replace exiting BB " << *exitBBToExitingBBMap[exitBasicBlock] << " with " << *exitingBasicBlock << " for exit BB " << *exitBasicBlock << " . Abort.\n";
        abort();
      }
      
      bool dominates = exitBasicBlockInfo->dom;
      if (!dominates){
        // Add check that the store was executed
        BasicBlock *checkBB = BasicBlock::Create(context, emptyName, storeInstFunc, exitBasicBlock);
        LoadInst *loadAlloca =
          new LoadInst(storePointerType, allocaInst, emptyName, checkBB);
        ICmpInst *cmpInst = new ICmpInst(*checkBB, ICmpInst::ICMP_NE, loadAlloca, constant0);
        CheckBasicBlock *checkBasicBlock = new CheckBasicBlock(checkBB, loadAlloca, cmpInst);

        // We need to create multiple instrumentation for the same store
        // because the branch inst will be different for all of them
        // depending on the exit basic block of the store inst loop
        entryCopy->allocPointer = loadAlloca;
        LoadStoreInstrumentation *loadStoreInstrumentation = getLoadStoreInstrumentation(M, entryCopy, funcToCallstack);

        // Add TEXAS instrumentation
        BasicBlock *instrumentBB = BasicBlock::Create(context, emptyName, storeInstFunc, checkBB); 
        InstrumentBasicBlock *instrumentBasicBlock = new InstrumentBasicBlock(instrumentBB, loadStoreInstrumentation);

        CheckInstrumentBasicBlock *checkInstrumentBasicBlock = new CheckInstrumentBasicBlock(checkBasicBlock, instrumentBasicBlock);
        exitBBToChainMap[exitBasicBlock].push_back(checkInstrumentBasicBlock);

      } else {
        Instruction *insertPoint = &(*(exitBasicBlock->getFirstInsertionPt()));
        LoadInst *loadAlloca = new LoadInst(storePointerType, allocaInst, emptyName, insertPoint);

        entryCopy->allocPointer = loadAlloca;
        LoadStoreInstrumentation *loadStoreInstrumentation = getLoadStoreInstrumentation(M, entryCopy, funcToCallstack);

        std::vector<Instruction *> instsToInsert = {loadStoreInstrumentation->pointerCast, loadStoreInstrumentation->addToState};
        insertInstructionsWithCheck(instsToInsert, loadAlloca);
      }
    }
  }

  return;
}

void addBasicBlockChain(std::unordered_map<BasicBlock*, std::vector<CheckInstrumentBasicBlock*>> &exitBBToChainMap, std::unordered_map<BasicBlock*, BasicBlock*> &exitBBToExitingBBMap){
  std::unordered_set<PHINode*> visitedPhiNodes;
  for (auto &elem : exitBBToChainMap){
    BasicBlock *exitBasicBlock = elem.first;
    if (exitBBToExitingBBMap.count(exitBasicBlock) == 0) {
      errs() << "ERROR: exit BB " << *exitBasicBlock << " does not have a corresponging exiting BB. Abort.\n";
      abort();
    }
    BasicBlock *exitingBasicBlock = exitBBToExitingBBMap[exitBasicBlock];

    std::vector<CheckInstrumentBasicBlock*> &checkInstrumentBasicBlock = elem.second;
    auto chainSize = checkInstrumentBasicBlock.size();
    for (auto i = 0; i < chainSize; ++i){
      if (i == 0){
        Instruction *exitingInst = exitingBasicBlock->getTerminator();
        if (exitingInst == nullptr){
          errs() << "ERROR: basic block " << exitingBasicBlock->front() << " is not well formed. Abort.\n";
          abort();
        }
        exitingInst->replaceSuccessorWith(exitBasicBlock, checkInstrumentBasicBlock[i]->checkBB->basicBlock);
      }

      BasicBlock *nextBB = exitBasicBlock;
      if (i < (chainSize - 1)){
        nextBB = checkInstrumentBasicBlock[i+1]->checkBB->basicBlock;
      }

      BranchInst *branchInstCheck = BranchInst::Create(checkInstrumentBasicBlock[i]->instrumentBB->basicBlock, nextBB, checkInstrumentBasicBlock[i]->checkBB->iCmpInst, checkInstrumentBasicBlock[i]->checkBB->basicBlock);

      llvm::IRBuilder<> builder(checkInstrumentBasicBlock[i]->instrumentBB->basicBlock);
      BranchInst *branchInstInstrumentation = cast<BranchInst>(builder.CreateBr(nextBB));
      LoadStoreInstrumentation *loadStoreInstrumentation = checkInstrumentBasicBlock[i]->instrumentBB->loadStoreInstrumentation;
      std::vector<Instruction *> instsToInsert = {loadStoreInstrumentation->pointerCast, loadStoreInstrumentation->addToState};
      Instruction *insertPoint = branchInstInstrumentation;
      insertInstructionsWithCheck(instsToInsert, insertPoint);
    }

    // Keep PHINodes coherent
    BasicBlock *lastCheckBB = checkInstrumentBasicBlock[chainSize-1]->checkBB->basicBlock;
    BasicBlock *lastInstrumentBB = checkInstrumentBasicBlock[chainSize-1]->instrumentBB->basicBlock;
    
    for (auto &inst : *exitBasicBlock){
      PHINode *phiNode = dyn_cast<PHINode>(&inst);
      if (phiNode == nullptr){
        break;
      }

      // Check if PHINode already patched
      if (visitedPhiNodes.count(phiNode)){
        continue;
      }
      visitedPhiNodes.insert(phiNode);

      Value *incomingValue = phiNode->getIncomingValueForBlock(exitingBasicBlock);
      phiNode->replaceIncomingBlockWith(exitingBasicBlock, lastCheckBB);
      phiNode->addIncoming(incomingValue, lastInstrumentBB);
    }
  }

  return;
}

void instrumentLoopLoadStoreAddresses(Module &M, std::map<Function*, Instruction*> &funcToCallstack, std::unordered_map<Instruction*, Entry*> &instToEntryMap, std::unordered_map<LoadStoreInstInfo*, std::unordered_set<ExitBasicBlockInfo*>> &instsToInstrument){
  // Instrument stores only once
  std::unordered_map<BasicBlock*, std::vector<CheckInstrumentBasicBlock*>> exitBBToChainMap;
  std::unordered_map<BasicBlock*, BasicBlock*> exitBBToExitingBBMap;
  instrumentLoopLoadStoreAddress(M, instsToInstrument, funcToCallstack, instToEntryMap, exitBBToChainMap, exitBBToExitingBBMap);
  addBasicBlockChain(exitBBToChainMap, exitBBToExitingBBMap);

  return;
}
