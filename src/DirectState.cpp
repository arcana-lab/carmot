#include "./include/DirectState.hpp"
#include "./include/ROIManager.hpp"
#include "./include/LoadStoreInstrumentation.hpp"
#include "llvm/IR/Instructions.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <vector>

#include "srcCloneFunc/include/MetadataInstruction.hpp"
#include "srcCloneFunc/include/TagROIs.hpp"

bool isConnectedToLoopGovIndVar(Value *pointerOperand, InductionVariable *loopGovIndVar){
  GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(pointerOperand);
  // Pointer operand not a gep
  if (gep == nullptr){
    errs() << "DEBUG: isInput() inst " << *pointerOperand << " not a gep\n";
    return false;
  }

  // We can't handle multidimensional arrays yet, we only promote a single ind var
  if (gep->getNumOperands() != 2){
    errs() << "DEBUG: isInput() inst " << *gep << " more than 2 operands, it has " << gep->getNumOperands() << "\n";
    return false;
  }

  Value *index = gep->getOperand(1);
  Instruction *indexAsInst = dyn_cast<Instruction>(index);
  // Index must be an Instruction to be an ind var
  if (indexAsInst == nullptr){
    errs() << "DEBUG: isInput() inst " << *index << " not an Instruction\n";
    return false;
  }

  std::unordered_set<Instruction*> loopGovIndVarInsts = loopGovIndVar->getAllInstructions();
  // Index is not connected to the ind var
  if (loopGovIndVarInsts.count(indexAsInst) == 0){
    errs() << "DEBUG: isInput() inst " << *indexAsInst << " not a connected to ind var\n";
    return false;
  }

  return true;
}

bool isInput(Instruction *inst, PDG *loopDG, LoopStructure *LS){
  LoadInst *loadInst = dyn_cast<LoadInst>(inst);
  // Not a load inst
  if (loadInst == nullptr){
    errs() << "DEBUG: isInput() inst " << *inst << " not a load\n";
    return false;
  }

  auto iterDep = [LS](Value *src, DGEdge<Value> *dep) -> bool {
    // If the dependence is not in the loop, then we don't care
    Instruction *srcAsInst = dyn_cast<Instruction>(src);
    if (srcAsInst != nullptr){
      if (!LS->isIncluded(srcAsInst)){
        return false;
      }
    }

    if (dep->isControlDependence()) {
      Value *headerTerminator = LS->getHeader()->getTerminator();
      if (src != headerTerminator){
        errs() << "DEBUG: isInput() iterDep() control dep\n";
        return true;
      } else {
        return false;
      }
    }

    CallBase *callbase = dyn_cast<CallBase>(src);
    if (callbase != nullptr){
      Function *calledFunc = callbase->getCalledFunction();
      if (calledFunc != nullptr){
        //errs() << "CALLEDF " << calledFunc->getName() << "\n";
        // llvm intrinsics do not modify memory (right?)
        if (calledFunc->isIntrinsic()){
          return false;
        }

        std::string calledFuncName = calledFunc->getName().str();
        if (calledFuncName.compare(MEMORYTOOL_START_TRACKING) == 0){
          return false;
        } else if (calledFuncName.compare(MEMORYTOOL_STOP_TRACKING) == 0){
          return false;
        } else if (calledFuncName.compare(MEMORYTOOL_CONTEXT) == 0){
          return false;
        }
      }
    }

    errs() << "DEBUG: isInput() iterDep() end. Analysing " << *src << "\n";
    return true;
  };
 
  bool isThereAnOutgoingControlOrMemoryDependence = loopDG->iterateOverDependencesFrom(inst, true, true, false, iterDep);
  if (isThereAnOutgoingControlOrMemoryDependence){
    errs() << "DEBUG: isInput() inst " << *inst << " has outgoing dep\n";
    return false;
  }

  bool isThereAnIncomingControlOrMemoryDependence = loopDG->iterateOverDependencesTo(inst, true, true, false, iterDep);
  if (isThereAnIncomingControlOrMemoryDependence){
    errs() << "DEBUG: isInput() inst " << *inst << " has incoming dep\n";
    return false;
  }

  return true;
}

bool isOutput(Instruction *inst, PDG *loopDG, LoopStructure *LS){
  StoreInst *storeInst = dyn_cast<StoreInst>(inst);
  // Not a store inst
  if (storeInst == nullptr){
    errs() << "DEBUG: isOutput() inst " << *inst << " not a store\n";
    return false;
  }

  auto iterDep = [LS](Value *src, DGEdge<Value> *dep) -> bool {
    // If the dependence is not in the loop, then we don't care
    Instruction *srcAsInst = dyn_cast<Instruction>(src);
    if (srcAsInst != nullptr){
      if (!LS->isIncluded(srcAsInst)){
        return false;
      }
    }

    if (dep->isControlDependence()) {
      Value *headerTerminator = LS->getHeader()->getTerminator();
      if (src != headerTerminator){
        errs() << "DEBUG: isOutput() iterDep() control dep\n";
        return true;
      } else {
        return false;
      }
    }

    CallBase *callbase = dyn_cast<CallBase>(src);
    if (callbase != nullptr){
      Function *calledFunc = callbase->getCalledFunction();
      if (calledFunc != nullptr){
        //errs() << "CALLEDF " << calledFunc->getName() << "\n";
        // llvm intrinsics do not modify memory (right?)
        if (calledFunc->isIntrinsic()){
          return false;
        }

        std::string calledFuncName = calledFunc->getName().str();
        if (calledFuncName.compare(MEMORYTOOL_START_TRACKING) == 0){
          return false;
        } else if (calledFuncName.compare(MEMORYTOOL_STOP_TRACKING) == 0){
          return false;
        } else if (calledFuncName.compare(MEMORYTOOL_CONTEXT) == 0){
          return false;
        }
      }
    }

    errs() << "DEBUG: isOutput() iterDep() end. Analysing " << *src << "\n";
    return true;
  };

  bool isThereAnOutgoingControlOrMemoryDependence = loopDG->iterateOverDependencesFrom(inst, true, true, false, iterDep);
  if (isThereAnOutgoingControlOrMemoryDependence){
    errs() << "DEBUG: isOutput() inst " << *inst << " has outgoing dep\n";
    return false;
  }

  bool isThereAnIncomingControlOrMemoryDependence = loopDG->iterateOverDependencesTo(inst, true, true, false, iterDep);
  if (isThereAnIncomingControlOrMemoryDependence){
    errs() << "DEBUG: isOutput() inst " << *inst << " has incoming dep\n";
    return false;
  }

  return true;
}

std::vector<DirectStateLoadStore> getDirectStateLoadStore(Noelle &noelle, StayConnectedNestedLoopForestNode *L, std::unordered_set<Instruction*> &instsToSkip, std::unordered_map<Instruction*, Entry*> &instToEntryMap){
  std::vector<DirectStateLoadStore> instsToInstrument;

  LoopDependenceInfo *LDI = noelle.getLoop(L);
  PDG *loopDG = LDI->getLoopDG();
  LoopStructure *LS = L->getLoop();
  InductionVariableManager *loopInductionVariableManager = LDI->getInductionVariableManager();
  InductionVariable *loopGovIndVar = loopInductionVariableManager->getLoopGoverningInductionVariable(*LS);
  auto loopGovIVAttribution = LDI->getLoopGoverningIVAttribution();
 
  std::unordered_set<BasicBlock *> loopBasicBlocks = LS->getBasicBlocks();
  // For every loop instruction
  for (BasicBlock *loopBasicBlock : loopBasicBlocks){
    for (Instruction &loopInstruction : *loopBasicBlock){
      // If it isn't in the map, then it's not a load or a store, skip it
      if (instToEntryMap.count(&loopInstruction) == 0){
        errs() << "DEBUG: getDirectState() inst not in map " << loopInstruction << "\n";
        continue;
      }

      if (instsToSkip.count(&loopInstruction)){
        errs() << "DEBUG: getDirectState() inst to skip " << loopInstruction << "\n";
        continue;
      }

      errs() << "DEBUG: getDirectState() considering inst " << loopInstruction << "\n";

      // When I figure out the other conditions to set an FSA state, consider to tag the load/store
      // instruction with metadata containing the FSA state to later instrument it.
      if (isInput(&loopInstruction, loopDG, LS)){
        LoadInst *loadInst = cast<LoadInst>(&loopInstruction);
        Value *pointerOperand = loadInst->getPointerOperand();
        if (isConnectedToLoopGovIndVar(pointerOperand, loopGovIndVar)){
          GetElementPtrInst *gep = cast<GetElementPtrInst>(pointerOperand);
          instsToInstrument.push_back(DirectStateLoadStore(&loopInstruction, gep->getPointerOperand(), loopGovIndVar->getStartValue(), loopGovIVAttribution->getExitConditionValue(), STATE_ADD_INPUT));
        } else {
          instsToInstrument.push_back(DirectStateLoadStore(&loopInstruction, pointerOperand, nullptr, nullptr, STATE_ADD_INPUT));
        }

      } else if (isOutput(&loopInstruction, loopDG, LS)){
        StoreInst *storeInst = cast<StoreInst>(&loopInstruction);
        Value *pointerOperand = storeInst->getPointerOperand();
        if (isConnectedToLoopGovIndVar(pointerOperand, loopGovIndVar)){
          GetElementPtrInst *gep = cast<GetElementPtrInst>(pointerOperand); 
          instsToInstrument.push_back(DirectStateLoadStore(&loopInstruction, gep->getPointerOperand(), loopGovIndVar->getStartValue(), loopGovIVAttribution->getExitConditionValue(), STATE_ADD_OUTPUT));
        } else {
          instsToInstrument.push_back(DirectStateLoadStore(&loopInstruction, pointerOperand, nullptr, nullptr, STATE_ADD_OUTPUT));
        }
      }
    }
  }

  return instsToInstrument;
}

std::vector<DirectStateLoadStore> getDirectStateLoadStoreAll(Noelle &noelle, std::vector<ROI*> &rois, std::unordered_set<Instruction*> &instsToSkip, std::unordered_map<Instruction*, Entry*> &instToEntryMap){
  std::vector<DirectStateLoadStore> instsToInstrument;

  for (ROI *roi : rois){
    CallBase *callToStartTracking = roi->start;
    StayConnectedNestedLoopForestNode *roiLoop = getROILoop(noelle, callToStartTracking);
    // Check if ROI is in a loop, if it is not, continue
    if (roiLoop == nullptr){
      continue;
    }

    // Check if ROI has PROMOTE_ALLOCA metadata tag
    if (!hasMetadata(callToStartTracking, PROMOTE_ALLOCAS)){
      continue;
    }

    std::vector<DirectStateLoadStore> instsToInstrumentTmp = getDirectStateLoadStore(noelle, roiLoop, instsToSkip, instToEntryMap);
    errs() << "DEBUG: roi " << *callToStartTracking << " directStateToInstrument size " << instsToInstrumentTmp.size() << "\n";
    instsToInstrument.insert(instsToInstrument.end(), instsToInstrumentTmp.begin(), instsToInstrumentTmp.end());
    //std::set_union(instsToInstrumentTmp.begin(), instsToInstrumentTmp.end(), instsToInstrument.begin(), instsToInstrument.end(), std::inserter(instsToInstrument, instsToInstrument.begin()));
  }

  return instsToInstrument;
}

std::unordered_set<Instruction*> getDirectStateLoadStoreInsts(std::vector<DirectStateLoadStore> &directStateToInstrumentAll){
  std::unordered_set<Instruction*> directStateToInstrumentInsts;
  for (auto &elem : directStateToInstrumentAll){
    directStateToInstrumentInsts.insert(elem.inst);
  }

  return directStateToInstrumentInsts;
}

std::vector<Instruction*> createCallToRuntime(Module &M, DirectStateLoadStore &elem, Entry *entry, std::map<Function *, Instruction *> &funcToCallstack){
  LLVMContext &context = M.getContext();
  Type *int64Type = Type::getInt64Ty(context);
  Type *int8PtrType = Type::getInt8PtrTy(context);

  Instruction *I = elem.inst;
  Value *addr = elem.baseAddr;
  auto parentFunc = I->getFunction();

  std::vector<Value*> callVals; // = {IDConstant, pointerCast, startIndVarCast, stopIndVarCast, elementBitwidthConst, FSAStateConst, callstackInst};
  std::vector<Instruction *> instsToInsert; // = {pointerCast, startIndVarCast, stopIndVarCast, runtimeCall};

  uint64_t id = entry->id;
  Constant *IDConstant = ConstantInt::get(int64Type, id);
  callVals.push_back(IDConstant);
  CastInst *pointerCast = CastInst::CreatePointerCast(addr, int8PtrType);
  callVals.push_back(pointerCast);
  instsToInsert.push_back(pointerCast);
  if (elem.startIndVar != nullptr){
    CastInst *startIndVarCast = CastInst::CreateIntegerCast(elem.startIndVar, int64Type, true);
    callVals.push_back(startIndVarCast);
    instsToInsert.push_back(startIndVarCast);
  } else {
    Constant *startIndVarConst = ConstantInt::get(int64Type, 0);
    callVals.push_back(startIndVarConst);
  }
  if (elem.stopIndVar != nullptr){
    CastInst *stopIndVarCast = CastInst::CreateIntegerCast(elem.stopIndVar, int64Type, true);
    callVals.push_back(stopIndVarCast);
    instsToInsert.push_back(stopIndVarCast);
  } else {
    Constant *stopIndVarConst = ConstantInt::get(int64Type, 2);
    callVals.push_back(stopIndVarConst);
  }
  unsigned elementBitwidth = I->getType()->getScalarSizeInBits();
  if (StoreInst *storeInst = dyn_cast<StoreInst>(I)){
    elementBitwidth = storeInst->getValueOperand()->getType()->getScalarSizeInBits(); 
  }
  Constant *elementBitwidthConst = ConstantInt::get(int64Type, elementBitwidth);
  callVals.push_back(elementBitwidthConst);
  Constant *FSAStateConst = ConstantInt::get(int64Type, elem.FSAState);
  callVals.push_back(FSAStateConst);
  Instruction *callstackInst = funcToCallstack[parentFunc];
  callVals.push_back(callstackInst);

  ArrayRef<Value*> callArgs = ArrayRef<Value*>(callVals);

  Function *runtimeFunc = getRuntimeFunction(M, TEXAS_SET_STATE);
  CallInst *runtimeCall = CallInst::Create(runtimeFunc, callArgs);
  instsToInsert.push_back(runtimeCall);

  return instsToInsert;
}

bool instrumentDirectStateIorO(Module &M, std::map<Function *, Instruction *> &funcToCallstack, Entry *entry, DirectStateLoadStore &elem){
  // Get things we need.
  LLVMContext &context = M.getContext();
  Type *int1Type = Type::getInt1Ty(context);
  Type *int64Type = Type::getInt64Ty(context);
  Type *int8PtrType = Type::getInt8PtrTy(context);
  Twine emptyName = Twine("");

  Instruction *I = elem.inst;
  Value *addr = elem.baseAddr;

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
  errs() << "Instrumenting " << *I << " from " << parentFunc->getName() << "\n";

  ConstantPointerNull *nullptrConstant = ConstantPointerNull::get(pointerType);
  GlobalVariable *newAllocation = new GlobalVariable(M, type, false, GlobalValue::CommonLinkage, nullptrConstant);
  Constant *constantFalse = ConstantInt::get(int1Type, 0);
  Constant *constantTrue = ConstantInt::get(int1Type, 1);
  GlobalVariable *executedOnce = new GlobalVariable(M, int1Type, false, GlobalValue::CommonLinkage, constantFalse);

  // Split the store inst basic block in two.
  // The first instruction of the new basic block is the splitting instruction (i.e., store inst in our case).
  BasicBlock* oldBlock = I->getParent();
  BasicBlock* newBlock = oldBlock->splitBasicBlock(I, "");
  BasicBlock* wrapperBlock = BasicBlock::Create(context, emptyName, parentFunc, newBlock);


  // Populate oldBlock
  // At the end of the old basic block compare the store pointer with alloca (containing the old store pointer value).
  // Remove the last instruction from oldBlock.
  oldBlock->getTerminator()->removeFromParent();

  // Create the comparison.
  LoadInst *load = new LoadInst(pointerType, newAllocation, emptyName, oldBlock);
  ICmpInst *cmpInst = new ICmpInst(*oldBlock, ICmpInst::ICMP_EQ, addr, load);

  // Based on compare, branch to the instrumentation (i.e., wrapper basic block), or to the store and rest of the code.
  BranchInst* brInst = BranchInst::Create(newBlock, wrapperBlock, cmpInst, oldBlock); // TODO jump to newBlock directly only if FSAstate is I, if it's O then jump to executedWrapperBlock.



  // Populate the wrapperBlock.
  // Create call to runtime
  std::vector<Instruction*> instsToInsert = createCallToRuntime(M, elem, entry, funcToCallstack);

  // Inject branch to newBlock.
  BranchInst* resumeRunning = BranchInst::Create(newBlock, wrapperBlock);

  // Insert instrumentation in wrapper basic block.
  insertInstructionsWithCheck(instsToInsert, resumeRunning);

  // Create and insert store inst to store current base address to global
  StoreInst *storeBaseAddrToGlobal = new StoreInst(addr, newAllocation, &(*wrapperBlock->getFirstInsertionPt()));


  // If we are here we modified the bitcode
  return true;
}

bool instrumentDirectStateOorCO(Module &M, std::map<Function *, Instruction *> &funcToCallstack, Entry *entry, DirectStateLoadStore &elem){
  // Get things we need.
  LLVMContext &context = M.getContext();
  Type *int1Type = Type::getInt1Ty(context);
  Type *int64Type = Type::getInt64Ty(context);
  Type *int8PtrType = Type::getInt8PtrTy(context);
  Twine emptyName = Twine("");

  Instruction *I = elem.inst;
  Value *addr = elem.baseAddr;

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
  errs() << "Instrumenting " << *I << " from " << parentFunc->getName() << "\n";

  ConstantPointerNull *nullptrConstant = ConstantPointerNull::get(pointerType);
  GlobalVariable *newAllocation = new GlobalVariable(M, type, false, GlobalValue::CommonLinkage, nullptrConstant);
  Constant *constantFalse = ConstantInt::get(int1Type, 0);
  Constant *constantTrue = ConstantInt::get(int1Type, 1);
  GlobalVariable *executedOnce = new GlobalVariable(M, int1Type, false, GlobalValue::CommonLinkage, constantFalse);

  // Split the store inst basic block in two.
  // The first instruction of the new basic block is the splitting instruction (i.e., store inst in our case).
  BasicBlock* oldBlock = I->getParent();
  BasicBlock* newBlock = oldBlock->splitBasicBlock(I, "");
  BasicBlock* wrapperBlock = BasicBlock::Create(context, emptyName, parentFunc, newBlock);
  BasicBlock* executedWrapperBlock = BasicBlock::Create(context, emptyName, parentFunc, newBlock);
  BasicBlock* wrapperBlock2 = BasicBlock::Create(context, emptyName, parentFunc, newBlock);



  // Populate oldBlock
  // At the end of the old basic block compare the store pointer with alloca (containing the old store pointer value).
  // Remove the last instruction from oldBlock.
  oldBlock->getTerminator()->removeFromParent();

  // Create the comparison.
  LoadInst *load = new LoadInst(pointerType, newAllocation, emptyName, oldBlock);
  ICmpInst *cmpInst = new ICmpInst(*oldBlock, ICmpInst::ICMP_EQ, addr, load);

  // Based on compare, branch to the instrumentation (i.e., wrapper basic block), or to the store and rest of the code.
  BranchInst* brInst = BranchInst::Create(executedWrapperBlock, wrapperBlock, cmpInst, oldBlock); // TODO jump to newBlock directly only if FSAstate is I, if it's O then jump to executedWrapperBlock.



  // Populate the wrapperBlock.
  // Create call to runtime
  elem.FSAState = STATE_ADD_OUTPUT;
  std::vector<Instruction*> instsToInsert = createCallToRuntime(M, elem, entry, funcToCallstack);

  // Inject branch to newBlock.
  BranchInst* resumeRunning = BranchInst::Create(newBlock, wrapperBlock);

  // Insert instrumentation in wrapper basic block.
  insertInstructionsWithCheck(instsToInsert, resumeRunning);

  // Create and insert store inst to store current base address to global
  StoreInst *storeBaseAddrToGlobal = new StoreInst(addr, newAllocation, &(*wrapperBlock->getFirstInsertionPt()));

  // Create and insert store inst to store current base address to global
  StoreInst *storeExecutedTrueToGlobal = new StoreInst(constantTrue, executedOnce, &(*wrapperBlock->getFirstInsertionPt()));



  // Populate executedWrapperBlock
  // Create the comparison.
  LoadInst *loadExecutedOnce = new LoadInst(int1Type, executedOnce, emptyName, executedWrapperBlock);
  ICmpInst *isExecutedOnce = new ICmpInst(*executedWrapperBlock, ICmpInst::ICMP_EQ, constantTrue, loadExecutedOnce);

  // Based on compare, branch to the instrumentation (i.e., wrapperBlock2), or to the store and rest of the code.
  BranchInst* brInstExecutedOnce = BranchInst::Create(wrapperBlock2, newBlock, cmpInst, executedWrapperBlock);



  // Populate the wrapperBlock2.
  // Create call to runtime
  elem.FSAState = STATE_ADD_CO;
  std::vector<Instruction*> instsToInsert2 = createCallToRuntime(M, elem, entry, funcToCallstack);

  // Inject branch to newBlock.
  BranchInst* resumeRunning2 = BranchInst::Create(newBlock, wrapperBlock2);

  // Insert instrumentation in wrapper basic block.
  insertInstructionsWithCheck(instsToInsert2, resumeRunning2);

  // Create and insert store inst to store current base address to global
  StoreInst *storeExecutedFalseToGlobal = new StoreInst(constantFalse, executedOnce, &(*wrapperBlock2->getFirstInsertionPt()));



  // If we are here we modified the bitcode
  return true;
}

bool instrumentDirectStateLoadStore(Module &M, std::map<Function *, Instruction *> &funcToCallstack, std::unordered_map<Instruction *, Entry *> &instToEntryMap, std::vector<DirectStateLoadStore> &instsToInstrument){
  bool modified = false;

  // If there are no stores to instrument, just return.
  if (instsToInstrument.size() == 0) {
    return false;
  }

  // Get things we need.
  LLVMContext &context = M.getContext();
  Type *int1Type = Type::getInt1Ty(context);
  Type *int64Type = Type::getInt64Ty(context);
  Type *int8PtrType = Type::getInt8PtrTy(context);

  for (auto &elem : instsToInstrument) {
    Instruction *I = elem.inst;
    if (instToEntryMap.count(I) == 0){
      errs() << "ERROR: instruction " << *I << " does not have entry. Abort.\n";
      abort();
    }

    Entry *entry = instToEntryMap[I];

    if (elem.FSAState == STATE_ADD_INPUT){ // it's an input (either variable or array)
      modified |= instrumentDirectStateIorO(M, funcToCallstack, entry, elem);
    } else if ((elem.FSAState == STATE_ADD_OUTPUT) && (elem.startIndVar != nullptr)){ // it's an output (array only)
      modified |= instrumentDirectStateIorO(M, funcToCallstack, entry, elem);
    } else if ((elem.FSAState == STATE_ADD_OUTPUT) && (elem.startIndVar == nullptr)){ // it's an output (variable only)
      modified |= instrumentDirectStateOorCO(M, funcToCallstack, entry, elem);
    }
  }

  return modified;
}

