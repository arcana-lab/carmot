#pragma once
#include <unordered_set>
#include <vector>
#include "./include/LoadStoreInstrumentation.hpp"
#include "./include/MostlyInvariant.hpp"

using namespace llvm;

void getModifiers(Instruction *ptr, LoopStructure *loopStructure, std::unordered_set<Instruction*> &modifiers, std::unordered_set<Instruction*> &visitedPHINodes){
  if (modifiers.count(ptr)){
    return;
  }

  if (!loopStructure->isIncluded(ptr)){
    return;
  }

  PHINode *phiPtr = dyn_cast<PHINode>(ptr);
  if (phiPtr == nullptr){ // Not a PHINode
    modifiers.insert(ptr);
    return;
  }

  // Handle PHINode
  if (visitedPHINodes.count(phiPtr)){
    return;
  }
  visitedPHINodes.insert(phiPtr);

  for (auto &incomingValue : phiPtr->incoming_values()){
    if (Instruction *incomingInst = dyn_cast<Instruction>(incomingValue)){
      getModifiers(incomingInst, loopStructure, modifiers, visitedPHINodes);
    } else {
      errs() << "WARNING: incoming PHINode value " << *incomingValue << " is not an Instruction.\n";
    }
  }

  return;
}

std::unordered_set<Instruction*> getLoopMostlyInvariants(Noelle &noelle, std::unordered_set<Instruction *> &instsToSkip, std::unordered_map<Instruction*,Entry*> &instToEntryMap) {
  // Get the profiler.
  auto hot = noelle.getProfiles();

  //This will hold the store instructions that can be optimized for mostly invariant use.
  std::unordered_set<Instruction*> mostlyInvariantStoreInstructions;

  // Get the set of stores that are mostly (but not) invariant.
  std::vector<LoopDependenceInfo*> *loops = noelle.getLoops();
  for(auto loop : *loops){
    LoopStructure *loopStructure = loop->getLoopStructure();
    std::unordered_set<BasicBlock *> loopBasicBlocks = loopStructure->getBasicBlocks();
    // Go over loop instructions.
    for(auto loopBasicBlock : loopBasicBlocks){
      for(auto &I : *loopBasicBlock){
        // If it is in the map, then it's either a load or a store
        if (instToEntryMap.count(&I) == 0){
          continue;
        }

        if (instsToSkip.count(&I)) {
          continue;
        }

        //StoreInst *storeInst = dyn_cast<StoreInst>(&I);
        //if (storeInst == nullptr){
        //  continue;
        //}

        Entry *entry = instToEntryMap[&I];

        /*
         * Fetch the address used by the store.
         */
        Value *addr = entry->allocPointer;

        /*
         * Skip invariants and (contributores to) induction variables.
         */
        Instruction *addrInst = dyn_cast<Instruction>(addr);
        if (addrInst == nullptr){
          continue;
        }

        /*
         * Found a potential almost-invariant.
         */
        std::unordered_set<Instruction *> modifiers;
        std::unordered_set<Instruction *> visitedPHINodes;
        getModifiers(addrInst, loopStructure, modifiers, visitedPHINodes);

        errs() << "MOSTLYINVARIANT? " << I << "\n";
        // Check if any modifier is in a hot path, if it is, then this store is NOT a mostly invariant.
        auto foundHot = false;
        auto instCounter = hot->getInvocations(&I);
        for (auto m : modifiers){
          errs() << "modifier " << *m << "\n";
          auto mCounter = hot->getInvocations(m);
          auto ratio = ((double)mCounter) / ((double)instCounter);
          if (ratio > 0.5){
            errs() << "MOSTLYINVARIANT? " << I << " ratio " << ratio << "\n";

            foundHot = true;
            break ;
          }
        }
        if (foundHot){
          continue ;
        }

        // If we're here, the current store is a mostly invariant one.
        mostlyInvariantStoreInstructions.insert(&I);
      }
    }
  }

  return mostlyInvariantStoreInstructions;
}

bool instrumentLoopMostlyInvariants(
    Module &M, std::map<Function *, Instruction *> &funcToCallstack,
    std::unordered_map<Instruction *, Entry *> &instToEntryMap,
    std::unordered_set<Instruction *> instsToInstrument) {
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

    // Add a stack location for old value of pointer to first BB and counter
    Type *storePointerType = addr->getType();
    if (!storePointerType->isPointerTy()){
      errs() << "ERROR: this " << *storePointerType << " is not a pointer type. Abort.\n";
      abort();
    }
    PointerType *pointerType = dyn_cast<PointerType>(storePointerType);
    auto parentFunc = I->getFunction();
    auto &firstBB = parentFunc->getEntryBlock();
    Instruction *insertPointAlloca = &(*(firstBB.getFirstInsertionPt()));
    
    ConstantPointerNull *nullptrConstant = ConstantPointerNull::get(pointerType);
    GlobalVariable *newAllocation = new GlobalVariable(M, pointerType, false, GlobalValue::CommonLinkage, nullptrConstant);
    //AllocaInst* newAllocation = new AllocaInst(storePointerType, 0, emptyName, insertPointAlloca);
    //AllocaInst* counterAllocation = new AllocaInst(int64Type, 0, emptyName, newAllocation);
    Constant *counterConstant0 = ConstantInt::get(int64Type, 0);
    GlobalVariable *counterAllocation = new GlobalVariable(M, int64Type, false, GlobalValue::CommonLinkage, counterConstant0);

    // Fill the new alloca with 0 the first time.
    //APInt value0(storePointerType->getPointerElementType()->getPrimitiveSizeInBits(), 0);
    //Constant *constant0 = Constant::getIntegerValue(storePointerType, value0);
    //APInt value0(dataLayout.getPointerTypeSizeInBits(storePointerType), 0);
    //Constant *constant0 = Constant::getIntegerValue(storePointerType, value0);
    //StoreInst* allocInit = new StoreInst(constant0, newAllocation, false);
    //allocInit->insertAfter(newAllocation);

    //Constant *counterConstant0 = ConstantInt::get(int64Type, 0);
    //StoreInst *counterInit = new StoreInst(counterConstant0, counterAllocation, false);
    //counterInit->insertAfter(counterAllocation);

    // Split the store inst basic block in two.
    // The first instruction of the new basic block is the splitting instruction (i.e., store inst in our case).
    BasicBlock* oldBlock = I->getParent();
    BasicBlock* newBlock = oldBlock->splitBasicBlock(I, "");
    BasicBlock* wrapperBlock = BasicBlock::Create(context, emptyName, parentFunc, newBlock);
    BasicBlock* lessThan2Block = BasicBlock::Create(context, emptyName, parentFunc, newBlock);
    BasicBlock* resetCounterBlock = BasicBlock::Create(context, emptyName, parentFunc, newBlock);

    // At the end of the old basic block compare the store pointer with alloca (containing the old store pointer value).
    // Remove the last instruction from oldBlock.
    oldBlock->getTerminator()->removeFromParent();
    // Create the comparison.
    LoadInst *load = new LoadInst(storePointerType, newAllocation, emptyName, oldBlock);
    ICmpInst *cmpInst = new ICmpInst(*oldBlock, ICmpInst::ICMP_EQ, addr, load);
    // Based on compare, branch to the instrumentation (i.e., wrapper basic block), or to the store and rest of the code.
    BranchInst* brInst = BranchInst::Create(lessThan2Block, resetCounterBlock, cmpInst, oldBlock);

    // Populate resetCounterBlock
    // Reset counter to 0
    Constant *constant0Int = ConstantInt::get(int64Type, 0);
    StoreInst *counterReset = new StoreInst(constant0Int, counterAllocation, resetCounterBlock);
    // Store new pointer into alloca we created.
    StoreInst* ptrStore = new StoreInst(addr, newAllocation, wrapperBlock);
    // Unconditional branch to wrapperBlock
    BranchInst* gotoWrapper = BranchInst::Create(wrapperBlock, resetCounterBlock);

    // Populate lessThan2Block
    LoadInst *counterLoad = new LoadInst(int64Type, counterAllocation, emptyName, lessThan2Block);
    Constant *constant2 = ConstantInt::get(int64Type, 2);
    ICmpInst *cmpInstLT2 = new ICmpInst(*lessThan2Block, ICmpInst::ICMP_ULT, counterLoad, constant2);
   // Branch to store BB
    BranchInst* brFromLT2 = BranchInst::Create(wrapperBlock, newBlock, cmpInstLT2, lessThan2Block);

    // Populate the wrapper block.
    // Add 1 to counter
    LoadInst *counterLoadAgain = new LoadInst(int64Type, counterAllocation, emptyName, wrapperBlock);
    Constant *constant1 = ConstantInt::get(int64Type, 1);
    BinaryOperator *add1 = BinaryOperator::Create(Instruction::Add, counterLoadAgain, constant1, emptyName, wrapperBlock);
    // Store new counter value to counter
    StoreInst *counterUpdate = new StoreInst(add1, counterAllocation, wrapperBlock);
    // Get instrumentation.
    LoadStoreInstrumentation *loadStoreInstrumentation =
        getLoadStoreInstrumentation(M, entry, funcToCallstack);
    // Inject branch to newBlock.
    BranchInst *gotoNewBlock = BranchInst::Create(newBlock, wrapperBlock);
    // Insert instrumentation in wrapper basic block.
    std::vector<Instruction *> instsToInsert = {loadStoreInstrumentation->pointerCast, loadStoreInstrumentation->addToState};
    Instruction *insertPoint = gotoNewBlock;
    insertInstructionsWithCheck(instsToInsert, insertPoint);

    // If we are here we modified the bitcode
    modified = true;
  }

  return modified;
}
