#pragma once 

#ifndef TAG_ROIS
#define TAG_ROIS

#include <string>
#include <cstdint>
#include <vector>
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstVisitor.h"
#include "../srcCARMOT/include/Utils.hpp"

#define ROI_ID "ROI_ID"
#define PROMOTE_ALLOCAS "PROMOTE_ALLOCAS"

using namespace llvm;

struct ROIInstVisitor : public InstVisitor<ROIInstVisitor> {
  std::vector<Instruction*> roiStartCallBases;
  std::vector<Instruction*> roiStartCallBasesCloned;
  Function *roiStartFunc;
  std::string funcNameSuffix = "_cloned";

  ROIInstVisitor(Module &M, std::string roiStartFuncName){
    this->roiStartFunc = M.getFunction(StringRef(roiStartFuncName));
  }

  void visitCallBase(CallBase &inst) {
    Function *callBaseFunc = inst.getCalledFunction();
    if (callBaseFunc == nullptr){
      return;
    }

    if (callBaseFunc != this->roiStartFunc){
      return;
    }

    Function *caller = inst.getFunction();
    std::string callerName = caller->getName().str();
    if (ends_with(callerName, funcNameSuffix)){
      this->roiStartCallBasesCloned.push_back(&inst);
    } else {
      this->roiStartCallBases.push_back(&inst);
    }

    return;
  }

};


bool tagROIsWithIDs(Module &M, std::string roiStartFuncName);

bool tagROIToPromoteAllocas(Instruction *roiStartInst, std::string promoteAllocas);
bool tagROIsToPromoteAllocas(Module &M, std::string roiStartFuncName);

#endif
