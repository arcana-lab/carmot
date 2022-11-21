#pragma once

#ifndef DIRECT_STATE
#define DIRECT_STATE

#include "noelle/core/Noelle.hpp"
#include "Utils.hpp"
#include "ROI.hpp"

#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include <vector>

using namespace llvm ;
using namespace llvm::noelle ;

enum operations {
  FREE_SLOT = 0,
  ALLOCATING,
  ALLOC_ADD,
  ALLOC_FREE,
  ESCAPE_ADD,
  STATE_ADD_READ,
  STATE_ADD_WRITE,
  STATE_ADD_INPUT,
  STATE_ADD_IO,
  STATE_ADD_CIO,
  STATE_ADD_TIO,
  STATE_ADD_OUTPUT,
  STATE_ADD_CO,
  STATE_ADD_TO,
  STATE_BEGIN,
  STATE_END,
  STATE_REGION_SET,
  THREAD_KILL,
  REDUNDANT,
  STATE_COMMIT,
  DETECT_CYCLE,
};

class DirectStateLoadStore {
  public:
    Instruction *inst;
    Value *baseAddr;
    Value *startIndVar;
    Value *stopIndVar;
    operations FSAState;

    DirectStateLoadStore(Instruction *inst, Value *baseAddr, Value *startIndVar, Value *stopIndVar, operations FSAState):inst{inst},baseAddr{baseAddr},startIndVar{startIndVar},stopIndVar{stopIndVar},FSAState{FSAState}{}
};

std::vector<DirectStateLoadStore> getDirectStateLoadStoreAll(Noelle &noelle, std::vector<ROI*> &rois, std::unordered_set<Instruction*> &instsToSkip, std::unordered_map<Instruction*, Entry*> &instToEntryMap);
bool instrumentDirectStateLoadStore(Module &M, std::map<Function *, Instruction *> &funcToCallstack, std::unordered_map<Instruction *, Entry *> &instToEntryMap, std::vector<DirectStateLoadStore> &instsToInstrument);
std::unordered_set<Instruction*> getDirectStateLoadStoreInsts(std::vector<DirectStateLoadStore> &directStateToInstrumentAll);
#endif
