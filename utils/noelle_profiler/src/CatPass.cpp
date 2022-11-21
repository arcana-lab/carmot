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
#include "llvm/IR/Attributes.h"
#include <unordered_set>
#include <utility>
#include <algorithm>

#include "Utils.hpp"
#include "noelle/core/Noelle.hpp"

using namespace llvm::noelle;

namespace {

struct CAT : public ModulePass {

  static char ID;

  CAT() : ModulePass(ID) {}

  struct MyInstVisitor : public InstVisitor<MyInstVisitor> {
    std::unordered_set<Instruction*> loadInsts;
    std::unordered_set<Instruction*> storeInsts;
    std::unordered_set<Instruction*> pinStartInsts;
    std::unordered_set<Instruction*> pinStopInsts;
    Function *loadInstrumentation;
    Function *storeInstrumentation;
    Function *pinStartTracking;
    Function *pinStopTracking;

    MyInstVisitor(Module &M){
      this->loadInstrumentation = M.getFunction(TEXAS_ADD_WITH_INFO_LOAD);
      this->storeInstrumentation = M.getFunction(TEXAS_ADD_WITH_INFO);
      this->pinStartTracking = M.getFunction(PIN_START_TRACKING);
      this->pinStopTracking = M.getFunction(PIN_STOP_TRACKING);
    }

    void visitCallInst(CallInst &inst) {
      Function *calledFunction = inst.getCalledFunction();
      if (calledFunction == this->loadInstrumentation){
        loadInsts.insert(&inst);
      } else if (calledFunction == this->storeInstrumentation){
        storeInsts.insert(&inst);
      } else if (calledFunction == this->pinStartTracking){
        pinStartInsts.insert(&inst);
      } else if (calledFunction == this->pinStopTracking){
        pinStopInsts.insert(&inst);
      }

      return;
    }
  };

  std::vector<std::pair<uint64_t, Instruction *>>
  profile(Noelle &noelle, std::unordered_set<Instruction *> &insts) {
    std::vector<std::pair<uint64_t, Instruction*>> ranked;

    auto hot = noelle.getProfiles();
    for (auto inst : insts) {
      auto invocations = hot->getInvocations(inst);
      ranked.push_back(std::make_pair(invocations, inst));
    }

    std::sort(ranked.begin(), ranked.end());

    return ranked;
  }

  void printVectorBackwards(std::vector<std::pair<uint64_t, Instruction*>> &ranked, uint64_t limit){
    if (limit == 0){
      limit = ranked.size();
    }

    errs() << "MEMORYTOOL: ranking instrumentation calls:\n";
    auto j = 0;
    for (int i = (ranked.size() - 1); i >= 0; --i){
      if (j >= limit){
        break;
      }
      j += 1;

      auto &elem = ranked[i];

      /* Getting debug info (DOES NOT WORK because of noelle-meta-clean)
      Instruction *inst = elem.second;
      CallInst *callInst = dyn_cast<CallInst>(inst);
      Value *argOperand1 = callInst->getArgOperand(1);
      Instruction *argOperandInst1 = dyn_cast<Instruction>(argOperand1);
      DILocation *diLocation = argOperandInst1->getDebugLoc();
      if (diLocation == nullptr){
        errs() << "NO DEBUG INFO\n";
        continue;
      }
      errs() << "invocations = " << elem.first << " instrumentation = " << *(elem.second) << " function = " << elem.second->getFunction()->getName() << " filename = " << diLocation->getFilename() << " linenum = " << diLocation->getLine() << " columnnum = " << diLocation->getColumn() << "\n";
      */

      errs() << "invocations = " << elem.first << " instrumentation = " << *(elem.second) << " function = " << elem.second->getFunction()->getName() << "\n";
    }

    return;
  }

  bool runOnModule(Module &M) override {
    bool modified = false;
    errs() << "NOLLE_PROFILER\n";

    // Fetch Noelle
    auto &noelle = getAnalysis<Noelle>();

    // Fetch all store inst
    MyInstVisitor IV(M);
    IV.visit(M);

    // Use the profiler to rank instrumentation calls
    std::vector<std::pair<uint64_t, Instruction*>> rankedLoadInst = profile(noelle, IV.loadInsts);
    std::vector<std::pair<uint64_t, Instruction*>> rankedStoreInst = profile(noelle, IV.storeInsts);
    std::vector<std::pair<uint64_t, Instruction*>> rankedPinStartInst = profile(noelle, IV.pinStartInsts);
    std::vector<std::pair<uint64_t, Instruction*>> rankedPinStopInst = profile(noelle, IV.pinStopInsts);
    
    // Print ranking
    uint64_t limit = 10;

    errs() << "LOAD\n";
    printVectorBackwards(rankedLoadInst, limit);
    errs() << "STORE\n";
    printVectorBackwards(rankedStoreInst, limit);

    errs() << "PIN START\n";
    printVectorBackwards(rankedPinStartInst, limit);
    errs() << "PIN STOP\n";
    printVectorBackwards(rankedPinStopInst, limit);


    return modified;
  }

  // We don't modify the program, so we preserve all analyses.
  // The LLVM IR of functions isn't ready at this point
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<Noelle>();

    return;
  }

}; // end of struct CAT

} // end of anonymous namespace

char CAT::ID = 0;
static RegisterPass<CAT> X("CAT", "CAT pass");

// Next there is code to register your pass to "clang"
static CAT *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
                                        [](const PassManagerBuilder &,
                                           legacy::PassManagerBase &PM) {
                                          if (!_PassMaker) {
                                            PM.add(_PassMaker = new CAT());
                                          }
                                        }); // ** for -Ox
static RegisterStandardPasses
    _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
              [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
                if (!_PassMaker) {
                  PM.add(_PassMaker = new CAT());
                }
              }); // ** for -O0
