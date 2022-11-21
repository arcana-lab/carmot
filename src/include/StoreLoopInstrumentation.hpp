#pragma once

#ifndef STORE_LOOP_INSTRUMENTATION
#define STORE_LOOP_INSTRUMENTATION

#include "Utils.hpp"
#include "LoadStoreInstrumentation.hpp"

using namespace llvm::noelle ;

class CheckBasicBlock{
  public:
    BasicBlock *basicBlock;
    LoadInst *loadInst;
    ICmpInst *iCmpInst;

    CheckBasicBlock(BasicBlock *basicBlock, LoadInst *loadInst, ICmpInst *iCmpInst):basicBlock{basicBlock},loadInst{loadInst},iCmpInst{iCmpInst}{}

};

class InstrumentBasicBlock{
  public:
    BasicBlock *basicBlock;
    LoadStoreInstrumentation *loadStoreInstrumentation;

    InstrumentBasicBlock(BasicBlock *basicBlock, LoadStoreInstrumentation *loadStoreInstrumentation):basicBlock{basicBlock},loadStoreInstrumentation{loadStoreInstrumentation}{}

};

class CheckInstrumentBasicBlock{
  public:
    CheckBasicBlock *checkBB;
    InstrumentBasicBlock *instrumentBB;

    CheckInstrumentBasicBlock(CheckBasicBlock *checkBB, InstrumentBasicBlock *instrumentBB):checkBB{checkBB},instrumentBB{instrumentBB}{}

};

class LoadStoreInstInfo{
  public:
    Instruction *inst;
    BasicBlock *loopPreHeaderBasicBlock;

    LoadStoreInstInfo(Instruction *inst, BasicBlock *loopPreHeaderBasicBlock):inst{inst},loopPreHeaderBasicBlock{loopPreHeaderBasicBlock}{}

};

class ExitBasicBlockInfo{
  public:
    BasicBlock *exitingBasicBlock;
    BasicBlock *exitBasicBlock;
    bool dom;

    ExitBasicBlockInfo(BasicBlock* exitingBasicBlock, BasicBlock *exitBasicBlock, bool dom):exitingBasicBlock{exitingBasicBlock},exitBasicBlock{exitBasicBlock},dom{dom}{}

};

void getLoadStoreInstExitBasicBlock(Noelle &noelle, Instruction *inst, LoopDependenceInfo *loop, std::unordered_map<LoadStoreInstInfo *, std::unordered_set<ExitBasicBlockInfo *>> &loadsStoresToInstrument);
void instrumentLoopLoadStoreAddresses(Module &M, std::map<Function*, Instruction*> &funcToCallstack, std::unordered_map<Instruction*, Entry*> &instToEntryMap, std::unordered_map<LoadStoreInstInfo*, std::unordered_set<ExitBasicBlockInfo*>> &instsToInstrument);

#endif
