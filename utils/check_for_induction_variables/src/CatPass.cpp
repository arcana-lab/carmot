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

#include "include/CheckForInductionVariables.hpp"
#include "srcPromoteAllocas/include/PromoteAllocaToReg.hpp"
#include "srcCloneFunc/include/TagROIs.hpp"
#include "srcCloneFunc/include/MetadataInstruction.hpp"

#define MAIN "main"

using namespace llvm;

namespace {

struct CAT : public ModulePass {

  static char ID;

  CAT() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    errs() << "CHECK_FOR_INDUCTION_VARIABLES\n";

#ifdef MEMORYTOOL_DISABLE_CHECKFORIV
    return false;
#endif

    bool modified = false;

    // Fetch Noelle
    auto &noelle = getAnalysis<Noelle>();

    // Get function to loops map
    std::unordered_map<Function*, std::unordered_map<Instruction*, StayConnectedNestedLoopForestNode*>> functionToLoopsMap = getFunctionToLoopsMap(M, noelle);
    //errs() << "DEBUG: functionToLoopsMap.size() = " << functionToLoopsMap.size() << "\n";

    // Check if promoted allocas are induction variables
    std::unordered_set<Instruction*> roisToPromoteAllocasSetAll;
    for (auto elem : functionToLoopsMap){
      Function *func = elem.first;
      std::unordered_map<Instruction*, StayConnectedNestedLoopForestNode*> &roiToLoopMap = elem.second;
      std::vector<Instruction*> roisToPromoteAllocas = checkForInductionVariables(noelle, roiToLoopMap);
      //errs() << "DEBUG: roisToPromoteAllocas.size() = " << roisToPromoteAllocas.size() << "\n";
      std::unordered_set<Instruction*> roisToPromoteAllocasSet(roisToPromoteAllocas.begin(), roisToPromoteAllocas.end()); // Inefficient AF, but don't care
      std::set_union(roisToPromoteAllocasSet.begin(), roisToPromoteAllocasSet.end(), roisToPromoteAllocasSetAll.begin(), roisToPromoteAllocasSetAll.end(), std::inserter(roisToPromoteAllocasSetAll, roisToPromoteAllocasSetAll.begin()));
    }

    // Find corresponding original ROIs to tag to promote allocas
    std::vector<Instruction*> roisToPromoteAllocasAll(roisToPromoteAllocasSetAll.begin(), roisToPromoteAllocasSetAll.end());
    //errs() << "DEBUG: roisToPromoteAllocasAll.size() = " << roisToPromoteAllocasAll.size() << "\n";
    std::vector<Instruction*> roisOriginalToPromoteAllocas = getOriginalROIsToPromoteAllocas(M, roisToPromoteAllocasAll);

    // Remove promote allocas tag from ROIs in cloned functions
    for (auto roiStartInstCloned : roisToPromoteAllocasAll){
      deleteMetadata(roiStartInstCloned, PROMOTE_ALLOCAS);
    }

    // Tag original ROIs to promote allocas
    for (auto roiStartInst : roisOriginalToPromoteAllocas){
      modified |= tagROIToPromoteAllocas(roiStartInst, "true");
    }

    std::vector<Function*> functionsToErase;
    for (auto &F : M){
      if (F.empty()){
        continue;
      }

      if (F.isIntrinsic()){
        continue;
      }

      std::string funcName = F.getName().str();
      if (ends_with(funcName, "_cloned")){
        functionsToErase.push_back(&F);
      }
    }

    for (auto elem : functionsToErase){
      elem->eraseFromParent();
      modified |= true;
    }

    return modified;
  }

  // We don't modify the program, so we preserve all analyses.
  // The LLVM IR of functions isn't ready at this point
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    //AU.setPreservesAll();
    
    // Required to promote alloca
    AU.addRequired<AssumptionCacheTracker>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.setPreservesCFG();
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
