#pragma once

#ifndef ROII
#define ROII

#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"

using namespace llvm;

class ROI{
  public:
    CallBase *start;
    std::vector<CallBase*> stops;

    ROI(CallBase *start, std::vector<CallBase*> &stops);
    DILocation* getInstDebugLoc(Instruction *inst);
    void print(void);
};

#endif
