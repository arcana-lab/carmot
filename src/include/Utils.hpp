#pragma once

#ifndef UTILS
#define UTILS

#include <memory>

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Pass.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/BreakCriticalEdges.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "noelle/core/Noelle.hpp"

#include "DebugData.hpp"

#define MAIN "main"
#define DELIMITER "/"

#define MEMORYTOOL_START_TRACKING "caratGetStateWrapper"
#define MEMORYTOOL_STOP_TRACKING "caratReportStateWrapper"
#define MEMORYTOOL_CONTEXT "endStateInvocationWrapper"

#define TEXAS_REMOVE "_Z42caratStateRemoveFromAllocationTableWrapperPv"
#define TEXAS_MALLOC "_Z22caratStateAllocWrappermPvmS_"
#define TEXAS_CALLOC "_Z23caratStateCallocWrappermPvmmS_"
#define TEXAS_REALLOC "_Z24caratStateReallocWrappermPvS_mS_"
#define TEXAS_ADD "_Z22caratAddToStateWrapperPv"
#define TEXAS_ADD_WITH_INFO "_Z30texasAddToStateWithInfoWrappermPvmmS_"
#define TEXAS_ADD_WITH_INFO_LOAD "_Z19texasAddToStateLoadmPvS_"
#define TEXAS_ADD_ESCAPE "_Z28caratAddToEscapeTableWrapperPv"
#define TEXAS_SET_STATE "_Z14setDirectStatemPvllmmS_"

#define MEMORYTOOL_PROLOGUE "_Z8prologueiiiiiiii" 
#define MEMORYTOOL_EPILOGUE "_Z16PrintJSONWrapperv" 

#define MEMORYTOOL_CALLSTACK "_Z12getBacktracev"
#define MEMORYTOOL_CALLSTACK_UNIQUE "_Z18getBacktraceUniquemm"
#define MEMORYTOOL_RMCALLSTACK "_Z15removeBacktracev"

#define PIN_START_TRACKING "startLogPinAll"
#define PIN_STOP_TRACKING "stopLogPinAll"
#define PIN_ADD "_Z23texasAddMultiToStatePinmPv"

#define OPTIMIZE_LOCALS "OPTIMIZE_LOCALS"
#define OPTIMIZE_LOCALS_KIND "OPTIMIZE_LOCALS_KIND"

using namespace llvm;
using namespace llvm::noelle ;

// Entry with source code info for globals, malloc, realloc, etc.
class Entry {
  public:
    Value *value = nullptr; // Either global variable or instruction
    Value *allocPointer = nullptr;
    Value *allocNewPointer = nullptr;
    Value *allocSize = nullptr;
    Value *allocNumElems = nullptr;
    uint64_t size = 0;
    uint64_t sizeElement = 0;
    std::string pathToFile = "";
    std::string varName = "";
    unsigned lineNum = 0;
    unsigned columnNum = 0;
    uint64_t id = 0;

    Entry(Value *value, Value *allocPointer, Value *allocNewPointer, Value *allocSize, Value *allocNumElems, uint64_t size, std::string pathToFile, std::string varName, unsigned lineNum, unsigned columnNum):value{value},allocPointer{allocPointer},allocNewPointer{allocNewPointer},allocSize{allocSize},allocNumElems{allocNumElems},size{size},pathToFile{pathToFile},varName{varName},lineNum{lineNum},columnNum{columnNum} {}

    DebugData getDebugData(void){
      DebugData debugData(this->pathToFile, this->varName, this->lineNum, this->columnNum);
      return debugData;
    }

    Entry* copy(void){
      Entry *entryCopy = new Entry(this->value, this->allocPointer, this->allocNewPointer, this->allocSize, this->allocNumElems, this->size, this->pathToFile, this->varName, this->lineNum, this->columnNum);
      entryCopy->id = this->id;
      return entryCopy;
    }
};

StayConnectedNestedLoopForestNode* getROILoop(Noelle &noelle, CallBase *startTrackingCall);
Function *getRuntimeFunction(Module &M, std::string funcName);
void insertInstructions(std::vector<Instruction *> &insts, Instruction *insertPoint, bool insertBefore);
void insertInstructionsWithCheck(std::vector<Instruction *> &insts, Instruction *insertPoint);
const DebugLoc &getDefaultDebugLoc(Value *value);
bool ends_with(std::string const & value, std::string const & ending);

#endif
