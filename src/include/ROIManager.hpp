#pragma once

#ifndef ROI_MANAGER
#define ROI_MANAGER

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Instructions.h"

#include <map>
#include <set>
#include <string>

#include "ROI.hpp"
#include "Utils.hpp"

using namespace llvm;
using namespace llvm::noelle;

struct CallInstVisitor : public InstVisitor<CallInstVisitor> {
  Function *functionOfInterest;
  std::map<Function*, std::set<CallBase*>> functionToCallsOfInterest;

  CallInstVisitor(Function *functionOfInterest);
  void visitCallBase(CallBase &inst);
};

class ROIManager{
  private:
    Function *startFunc;
    Function *stopFunc;
    Function *contextFunc;
    std::unordered_map<Function*, std::vector<ROI*>> funcToRoisMap;
    std::unordered_set<Instruction*> roisLoadsStores;

    std::vector<ROI*> visitROIs(std::map<Function*, std::set<CallBase*>> mapToStart);
    std::vector<ROI*> computeROIs(Function &F);
    std::unordered_set<Instruction*> computeROIsLoadsStores(void);

  public:
    
    ROIManager(Module &M);
    void print(void);
    std::vector<ROI*> getROIs(Module &M);
    std::vector<ROI*> getROIs(Function &F);
    std::unordered_set<Instruction*> getROIsLoadsStores(void);
};

#endif
