//This llvm pass is intended to be a final pass before 

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DerivedUser.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/IRBuilder.h"


#include <unordered_map>
#include <set>
#include <map>
#include <queue>
#include <deque>


#define DEBUG 0

#define DINA_INT 1 //This will determine if we are building temporal locality graph

//This will inject calls to build the rt tables
#define CARAT_STATE_OPT "_Z14OptimizeStatesv"


using namespace llvm;



void populateLibCallMap(std::unordered_map<std::string, int>* functionCalls){
    std::pair<std::string, int> call;

    call.first = "llvm.memcpy.p0i8.p0i8.i64";
    call.second = -3;
    functionCalls->insert(call);

    call.first = "llvm.memmove.p0i8.p0i8.i64";
    call.second = -3;
    functionCalls->insert(call);

}