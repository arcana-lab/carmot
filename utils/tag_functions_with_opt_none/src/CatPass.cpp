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

#include "../../../src/include/Utils.hpp"
#include "./include/AvoidLocalVariableInstrumentation.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <cxxabi.h>

//#define MEMORYTOOL_DISABLE_LOCALS_OPT

using namespace llvm;

namespace {

struct CAT : public ModulePass {

  static char ID;

  CAT() : ModulePass(ID) {}

  bool tagFunctionOptNone(Function *functionToTag){
    bool modified = false;
    functionToTag->addFnAttr(Attribute::NoInline);
    functionToTag->addFnAttr(Attribute::OptimizeNone);
    modified = true;

    return modified;
  }

  bool rmTagFunctionOptNone(Function *functionToTag){
    bool modified = false;
    functionToTag->removeFnAttr(Attribute::OptimizeNone);
    modified = true;

    return modified;
  }

  void tagFunctionWithMetadata(Function *functionToTag){
    auto &context = functionToTag->getContext();
    MDNode *metadataNode = MDNode::get(context, MDString::get(context, OPTIMIZE_LOCALS));
    functionToTag->setMetadata(OPTIMIZE_LOCALS_KIND, metadataNode);

    return;
  }

  bool tagFunctionsOptNone(Module &M, std::unordered_set<Function*> &functionsToTag){
    bool modified = false;
    for (auto &F : M){
      if (functionsToTag.count(&F)){
        modified |= tagFunctionOptNone(&F);
        tagFunctionWithMetadata(&F);
      } else {
        modified |= rmTagFunctionOptNone(&F);
      }
    }

    return modified;
  }

  bool runOnModule(Module &M) override {
    bool modified = false;

    std::unordered_set<Function*> functionsToTag;
#ifndef MEMORYTOOL_DISABLE_LOCALS_OPT
    // Fetch noelle
    auto &noelle = getAnalysis<Noelle>();
    functionsToTag = findFunctionsToInstrument(M, noelle);
#else
    functionsToTag = getAllFunctions(M);
#endif

    modified |= tagFunctionsOptNone(M, functionsToTag);

    errs() << "TAG_FUNCTIONS_WITH_OPT_NONE\n";
    for (auto elem : functionsToTag){
      errs() << "function: " << elem->getName() << "\n";
    }

    return modified;
  }

  // We don't modify the program, so we preserve all analyses.
  // The LLVM IR of functions isn't ready at this point
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    //AU.setPreservesAll();
#ifndef MEMORYTOOL_DISABLE_LOCALS_OPT
    AU.addRequired<Noelle>();
#endif

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
