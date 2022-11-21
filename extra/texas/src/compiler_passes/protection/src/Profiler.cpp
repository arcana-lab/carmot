//This llvm pass is intended to be a final pass before 

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
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

#include "Noelle.hpp"

#include <unordered_map>
#include <set>
#include <queue>
#include <deque>

#define DEBUG 0


#define STORE_GUARD 1
#define LOAD_GUARD 1
#define CALL_GUARD 1

//This implements intel MPX instructions. If this is 0 it will do bounds checking instead
#define CYCLE_GUARD 1 

#define OPTIMIZED 1 



using namespace llvm;

namespace {

  class SCEV_CARAT_Visitor : public SCEVVisitor<SCEV_CARAT_Visitor, Value*> {
    public:
      Value * visitConstant (const SCEVConstant *S) {
        return S->getValue();
      }
  };

  struct PROTECT : public ModulePass {
    static char ID; 

    PROTECT() : ModulePass(ID) {}

    bool doInitialization (Module &M) override {
      return false;
    }


    //This pass should go through all the functions and wrap
    //the memory instructions with the injected calls needed.	
    bool runOnModule (Module &M) override {
      std::set<std::string> functionsInProgram;
      std::unordered_map<string, int> functionCalls;
      bool modified = false;


      //COUNTERS 
      uint64_t redundantGuard = 0;
      uint64_t loopInvariantGuard = 0;
      uint64_t scalarEvolutionGuard = 0;
      uint64_t nonOptimizedGuard = 0;
      uint64_t callGuardOpt = 0;

      auto& noelle = getAnalysis<Noelle>();

      LLVMContext &MContext = M.getContext();
      Type* int64Type = Type::getInt64Ty(MContext);
      Type* int32Type = Type::getInt32Ty(MContext);
      Type* int64PtrType = Type::getInt64PtrTy(MContext, 0);
      ConstantInt* ptrNum = ConstantInt::get(MContext, llvm::APInt(/*nbits*/64, 0x22DEADBEEF22, /*bool*/false));
      Constant* numNowPtr = ConstantExpr::getIntToPtr(ptrNum, int64PtrType, false);

      std::unordered_map<Instruction*, pair<Instruction*, Value*>> storeInsts;
      std::map<Function*, BasicBlock*> functionToEscapeBlock;
      for(auto& F : M){
        if (F.empty()) {
          continue ;
        }

        /*
         * Define the GEN and KILL sets.
         */
        auto dfaGEN = [](Instruction *inst, DataFlowResult *res) -> void {

          /*
           * Handle store instructions.
           */
          if ( true
              && !isa<StoreInst>(inst)
              && !isa<LoadInst>(inst)
             ){
            return ;
          }
          Value *ptrGuarded = nullptr;

          if (isa<StoreInst>(inst)){
#if STORE_GUARD
            auto storeInst = cast<StoreInst>(inst);
            ptrGuarded = storeInst->getPointerOperand();
#endif
          } else {
#if LOAD_GUARD
            auto loadInst = cast<LoadInst>(inst);
            ptrGuarded = loadInst->getPointerOperand();
#endif
          }
          if (ptrGuarded == nullptr){
            return ;
          }

          /*
           * Fetch the GEN[inst] set.
           */
          auto& instGEN = res->GEN(inst);

          /*
           * Add the pointer guarded in the GEN[inst] set.
           */
          instGEN.insert(ptrGuarded);

          return ;
        };
        auto dfaKILL = [](Instruction *inst, DataFlowResult *res) -> void {
          return;
        };

        /*
         * Initialize IN and OUT sets.
         */
        std::set<Value *> universe;
        for(auto& B : F){
          for(auto& I : B){
            universe.insert(&I);
            for (auto i=0; i < I.getNumOperands(); i++){
              auto op = I.getOperand(i);
              if (isa<Function>(op)){
                continue ;
              }
              if (isa<BasicBlock>(op)){
                continue ;
              }
              universe.insert(op);
            }
          }
        }
        for (auto& arg : F.args()){
          universe.insert(&arg);
        }
        auto firstBB = &*F.begin();
        auto firstInst = &*firstBB->begin();
        auto initIN = [universe, firstInst] (Instruction *inst, std::set<Value *>& IN) -> void{
          if (inst == firstInst){
            return ;
          }
          IN = universe;
          return ;
        };
        auto initOUT = [universe](Instruction *inst, std::set<Value *>& OUT) -> void{
          OUT = universe;
          return ;
        };

        /*
         * Define the IN and OUT data flow equations.
         */
        auto computeIN = [] (Instruction *inst, std::set<Value *>& IN, Instruction *predecessor, DataFlowResult *df) -> void{
          auto& OUTPred = df->OUT(predecessor);

          std::set<Value *> tmpIN{};
          std::set_intersection(IN.begin(), IN.end(), OUTPred.begin(), OUTPred.end(),  std::inserter(tmpIN, tmpIN.begin()));
          IN = tmpIN;

          return ;
        };
        auto computeOUT = [] (Instruction *inst, std::set<Value *>& OUT, DataFlowResult *df) -> void{

          /*
           * Fetch the IN[inst] set.
           */
          auto& IN = df->IN(inst);

          /*
           * Fetch the GEN[inst] set.
           */
          auto& GEN = df->GEN(inst);

          /*
           * Set the OUT[inst] set.
           */
          OUT = IN;
          OUT.insert(GEN.begin(), GEN.end());

          return ;
        };

        /*
         * Apply the available carat DFA.
         */
        auto dfe = noelle.getDataFlowEngine();
        auto dfaResult = dfe.applyForward(&F, dfaGEN, dfaKILL, initIN, initOUT, computeIN, computeOUT);

        /* 
         * Print the data flow results.
         */
        /* 
           F.print(errs());
           errs() << "DFA: START\n";
           for(auto& B : F){
           for(auto& I : B){
           if (isa<TerminatorInst>(&I)){
           continue ;
           }
           errs() << "DFA:   Instruction \"" << I << "\"\n";
           auto& GEN = dfaResult->GEN(&I);
           for (auto ptrInGEN : GEN){
           errs() << "DFA:     GEN includes " << ptrInGEN << " \"" << *ptrInGEN << "\"\n";
           }

           auto& IN = dfaResult->IN(&I);
           for (auto ptrInIN : IN){
           errs() << "DFA:     IN includes \"" << *ptrInIN << "\"\n";
           }

           auto& OUT = dfaResult->OUT(&I);
           for (auto ptrInIN : OUT){
           errs() << "DFA:     OUT includes \"" << *ptrInIN << "\"\n";
           }

           }
           }
           errs() << "DFA: END\n";
           */

        /*
         * Define the code that will be executed to identify where to place the guards.
         */
        //errs() << "COMPILER OPTIMIZATION\n";
        //F.print(errs());
        auto& loopInfo = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
        auto& scalarEvo =  getAnalysis<ScalarEvolutionWrapperPass>(F).getSE();
        auto findPointToInsertGuard = [&scalarEvolutionGuard, &loopInvariantGuard, &nonOptimizedGuard, &redundantGuard, numNowPtr, dfaResult, &loopInfo, &scalarEvo, &storeInsts](Instruction *inst, Value *pointerOfMemoryInstruction) -> void {
#if OPTIMIZED
          /*
           * Check if the pointer has already been guarded.
           */
          auto& INSetOfI = dfaResult->IN(inst);
          if (INSetOfI.find(pointerOfMemoryInstruction) != INSetOfI.end()){
            //errs() << "YAY: skip a guard for the instruction: " << *inst << "\n";
            redundantGuard++;                  
            return ;
          }

          /*
           * We have to guard the pointer.
           *
           * Check if we can hoist the guard outside the loop.
           */
          //errs() << "XAN: we have to guard the memory instruction: " << *inst << "\n" ;
          auto added = false;
          auto nestedLoop = loopInfo.getLoopFor(inst->getParent());
          if (nestedLoop == nullptr){

            /*
             * We have to guard just before the memory instruction.
             */
            storeInsts[inst] = {inst, pointerOfMemoryInstruction};
            nonOptimizedGuard++;
            //errs() << "XAN:   The instruction doesn't belong to a loop so no further optimizations are available\n";
            return ;
          }
          //errs() << "XAN:   It belongs to the loop " << *nestedLoop << "\n";
          Instruction* preheaderBBTerminator;
          while (nestedLoop != NULL){
            if (  false
                || nestedLoop->isLoopInvariant(pointerOfMemoryInstruction)
                || isa<Argument>(pointerOfMemoryInstruction)
                || !isa<Instruction>(pointerOfMemoryInstruction)
                || !nestedLoop->contains(cast<Instruction>(pointerOfMemoryInstruction))
               ){
              //errs() << "YAY:   we found an invariant address to check:" << *inst << "\n";
              auto preheaderBB = nestedLoop->getLoopPreheader();
              preheaderBBTerminator = preheaderBB->getTerminator();
              nestedLoop = loopInfo.getLoopFor(preheaderBB);
              added = true;

            } else {
              break;
            }
          }
          if(added){

            /*
             * We can hoist the guard outside the loop.
             */
            loopInvariantGuard++;
            storeInsts[inst] = {preheaderBBTerminator, pointerOfMemoryInstruction};
            return ;
          }

          /*
           * The memory instruction isn't loop invariant.
           *
           * Check if it is based on a bounded scalar evolution.
           */
          //errs() << "XAN:   It cannot be hoisted. Check if it can be merged\n";
          auto scevPtrComputation = scalarEvo.getSCEV(pointerOfMemoryInstruction);
          //errs() << "XAN:     SCEV = " << *scevPtrComputation << "\n";
          if (auto AR = dyn_cast<SCEVAddRecExpr>(scevPtrComputation)){
            //errs() << "XAN:       It is SCEVAddRecExpr\n";

            /*
             * The computation of the pointer is based on a bounded scalar evolution.
             * This means, the guard can be hoisted outside the loop where the boundaries used in the check go from the lowest to the highest address.
             */
            SCEV_CARAT_Visitor visitor{};
            auto startAddress = numNowPtr; // FIXME Iterate over SCEV to fetch the actual start and end addresses
            //auto startAddress = visitor.visit((const SCEV *)AR);
            /*auto startAddressSCEV = AR->getStart();
              errs() << "XAN:         Start address = " << *startAddressSCEV << "\n";
              Value *startAddress = nullptr;
              if (auto startGeneric = dyn_cast<SCEVUnknown>(startAddressSCEV)){
              startAddress = startGeneric->getValue();
              } else if (auto startConst = dyn_cast<SCEVConstant>(startAddressSCEV)){
              startAddress = startConst->getValue();
              }*/
            if (startAddress){
              //errs() << "YAY: we found a scalar evolution-based memory instruction: " ;
              //inst->print(errs());
              //errs() << "\n";
              auto nestedLoop = AR->getLoop();
              auto preheaderBB = nestedLoop->getLoopPreheader();
              preheaderBBTerminator = preheaderBB->getTerminator();

              scalarEvolutionGuard++;

              storeInsts[inst] = {preheaderBBTerminator, startAddress};
              return;
            }
          }
#endif //OPTIMIZED
          //errs() << "NOOO: the guard cannot be hoisted or merged: " << *inst << "\n" ;

          /*
           * We have to guard just before the memory instruction.
           */
          storeInsts[inst] = {inst, pointerOfMemoryInstruction};
          nonOptimizedGuard++;
          return ;
        };

        /*
         * Check if there is no stack allocations other than those in the first basic block of the function.
         */
        auto allocaOutsideFirstBB = false;
        for(auto& B : F){
          for(auto& I : B){
            if (  true
                && isa<AllocaInst>(&I)
                && (&B != firstBB)
               ){

              /*
               * We found a stack allocation not in the first basic block.
               */
              //errs() << "NOOOO: Found an alloca outside the first BB = " << I << "\n";
              allocaOutsideFirstBB = true;

              break ;
            }
          }
        }

        /*
         * Identify where to place the guards.
         */
        std::unordered_map<Function *, bool> functionAlreadyChecked;
        for(auto& B : F){
          for(auto& I : B){
#if CALL_GUARD
            if(isa<CallInst>(I) || isa<InvokeInst>(I)){
#if !OPTIMIZED
              allocaOutsideFirstBB = true;
#endif

              Function *calleeFunction;
#if OPTIMIZED
              /*
               * Check if we are invoking an LLVM metadata callee. We don't need to guard these calls.
               */
              if (auto tmpCallInst = dyn_cast<CallInst>(&I)){
                calleeFunction = tmpCallInst->getCalledFunction();
              } else {
                auto tmpInvokeInst = cast<InvokeInst>(&I);
                calleeFunction = tmpInvokeInst->getCalledFunction();
              }
              if ( true
                  && (calleeFunction != nullptr)
                  && calleeFunction->isIntrinsic()
                 ){
                //errs() << "YAY: no need to guard calls to intrinsic LLVM functions: " << I << "\n" ;
                continue;
              }

              /*
               * Check if we have already checked the callee and there is no alloca outside the first basic block. We don't need to guard more than once a function in this case.
               */
              if (  true
                  && !allocaOutsideFirstBB
                  && (calleeFunction != nullptr)
                  && (functionAlreadyChecked[calleeFunction] == true)
                 ){
                //errs() << "YAY: no need to guard twice the callee of the instruction " << I << "\n" ;


                continue ;
              }
#endif

              /*
               * Check if we can hoist the guard.
               */
              if (allocaOutsideFirstBB){

                /*
                 * We cannot hoist the guard.
                 */
                storeInsts[&I] = {&I, numNowPtr};
                //errs() << "NOOO: missed optimization because alloca invocation outside the first BB for : " << I << "\n" ;
                nonOptimizedGuard++;
                continue ;
              }

              /*
               * We can hoist the guard because the size of the allocation frame is constant.
               *
               * We decided to place the guard at the beginning of the function. 
               * This could be good if we have many calls to this callee.
               * This could be bad if we have a few calls to this callee and they could not be executed.
               */
              //errs() << "YAY: we found a call check that can be hoisted: " << I << "\n" ;
              storeInsts[&I] = {firstInst, numNowPtr};
              functionAlreadyChecked[calleeFunction] = true;
              callGuardOpt++;
              continue ;
            }
#endif
#if STORE_GUARD
            if(isa<StoreInst>(I)){
              auto storeInst = cast<StoreInst>(&I);
              auto pointerOfStore = storeInst->getPointerOperand();
              findPointToInsertGuard(&I, pointerOfStore);
              continue ;
            }
#endif
#if LOAD_GUARD
            if(isa<LoadInst>(I)){
              auto loadInst = cast<LoadInst>(&I);
              auto pointerOfStore = loadInst->getPointerOperand();
              findPointToInsertGuard(&I, pointerOfStore);
              continue ;
            }
#endif
          }
        }

        /*
         * Print where to put the guards
         */
        //errs() << "GUARDS\n";
        for (auto& guard : storeInsts){
          auto inst = guard.first;
          //errs() << " " << *inst << "\n";
        }
      }

      ConstantInt* constantNum = ConstantInt::get(MContext, llvm::APInt(/*nbits*/64, 0, /*bool*/false));
      ConstantInt* constantNum2 = ConstantInt::get(MContext, llvm::APInt(/*nbits*/64, 0, /*bool*/false));
      ConstantInt* constantNum1 = ConstantInt::get(MContext, llvm::APInt(/*nbits*/64, (((uint64_t)pow(2, 64)) - 1), /*bool*/false));
      Type* voidType = Type::getVoidTy(MContext);
      Instruction* nullInst = nullptr;
      GlobalVariable* tempGlob = nullptr; 
      GlobalVariable* lowerBound = new GlobalVariable(M, int64Type, false, GlobalValue::CommonLinkage, constantNum2, "lowerBound", tempGlob, GlobalValue::NotThreadLocal, 0, false);
      GlobalVariable* upperBound = new GlobalVariable(M, int64Type, false, GlobalValue::ExternalLinkage, constantNum1, "upperBound", tempGlob, GlobalValue::NotThreadLocal, 0, false);
#if CYCLE_GUARD

      std::vector<Instruction*> tempRets;
      for(auto& F : M){
        if(F.getName() == "main"){
          for(auto& B : F){
            for(auto& I : B){
              if(isa<ReturnInst>(I)){
                LoadInst* retReturner = new LoadInst(int64Type, lowerBound, "", nullInst);
                retReturner->insertBefore(&I);
                CastInst* int32CastInst = CastInst::CreateIntegerCast(retReturner, int32Type, false, "", nullInst); 
                int32CastInst->insertBefore(&I);
                ReturnInst* retInst =  ReturnInst::Create(MContext, int32CastInst, nullInst);
                retInst->insertBefore(&I);
                tempRets.push_back(&I);
              }
            }
          }
        }
      }
      for(auto rets : tempRets){
        rets->eraseFromParent();
      }
      for(auto& myPair : storeInsts){
        auto I = myPair.second.first;
        StoreInst* singleCycleStore = new StoreInst(constantNum, lowerBound, true, I);
      }

#else
      Function* mainFunction = M.getFunction("main");
      if(mainFunction == NULL){
        //errs() << "Uh oh, no main\n";
        exit(1);
      }
      else{
        //StoreInst* upperSetter = new StoreInst(constantNum1, upperBound, mainFunction->getEntryBlock().getFirstNonPHI());

      }
      for(auto &myPair : storeInsts){
        auto I = myPair.second.first;
        auto addressToCheck = myPair.second.second;

        Function* storeFunc = I->getFunction();
        auto entry = functionToEscapeBlock.find(storeFunc);
        BasicBlock* escapeBlock;
        if(entry == functionToEscapeBlock.end()){
          escapeBlock = BasicBlock::Create(MContext, "", storeFunc);
          functionToEscapeBlock.insert({storeFunc,escapeBlock});


          std::vector<Type*> params;
          //params.push_back(int64Type); 
          ArrayRef<Type*> args = ArrayRef<Type*>(params);
          auto signature = FunctionType::get(voidType, args, false);                
          auto exitFunc = M.getOrInsertFunction("abort", signature);


          std::vector<Value*> callVals;
          //callVals.push_back(constantNum);
          ArrayRef<Value*> callArgs = ArrayRef<Value*>(callVals);
          CallInst* exitCall = CallInst::Create(exitFunc, callArgs, "", escapeBlock);
          UnreachableInst* unreach = new UnreachableInst(MContext, escapeBlock);
          /* Type* retType = storeFunc->getReturnType();

             if(retType->isVoidTy()){
             ReturnInst* retInst = ReturnInst::Create(MContext, escapeBlock);
             }
             else if(retType->isIntegerTy()){ 

             ConstantInt* intNum = ConstantInt::get(MContext, llvm::APInt(retType->getIntegerBitWidth(), 0, false));
             ReturnInst* retInst = ReturnInst::Create(MContext, intNum, escapeBlock);
             }
             else if(retType->isFloatTy()){ 
             double a = 0;
             Value* ap = ConstantFP::get(retType, a);
             ReturnInst* retInst = ReturnInst::Create(MContext, ap, escapeBlock);
             }
             else if(retType->isDoubleTy()){
             double a = 0;
             Value* ap = ConstantFP::get(retType, a);
             ReturnInst* retInst = ReturnInst::Create(MContext, ap, escapeBlock);
             }
             else if(retType->isPointerTy()){ 
             PointerType* ptrTy = (cast<PointerType>(retType));
             ReturnInst* retInst = ReturnInst::Create(MContext, ConstantPointerNull::get(ptrTy), escapeBlock);
             }
             */

        }
        else{
          escapeBlock = entry->second;
        }
        BasicBlock* oldBlock = I->getParent();
        BasicBlock* newBlock = oldBlock->splitBasicBlock(I, "");
        BasicBlock* oldBlock1 = BasicBlock::Create(MContext, "", oldBlock->getParent(), newBlock);

        Instruction* unneededBranch = oldBlock->getTerminator();
        if(isa<BranchInst>(unneededBranch)){
          unneededBranch->eraseFromParent();
        }
        Value* compareVal;
        Function* tFp;
        compareVal = addressToCheck;
        const Twine &NameString = ""; 
        Instruction* tempInst = nullptr; 
        CastInst* int64CastInst = CastInst::CreatePointerCast(compareVal, int64Type, NameString, tempInst);
        LoadInst* lowerCastInst = new LoadInst(int64Type, lowerBound, NameString, tempInst);
        LoadInst* upperCastInst = new LoadInst(int64Type, upperBound, NameString, tempInst);
        CmpInst* compareInst = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_ULT, int64CastInst, lowerCastInst, "", oldBlock);
        int64CastInst->insertBefore(compareInst);
        lowerCastInst->insertBefore(compareInst);
        BranchInst* brInst = BranchInst::Create(escapeBlock, oldBlock1, compareInst, oldBlock);

        //CastInst* int64CastInst1 = CastInst::CreatePointerCast(compareVal, int64Type, NameString, tempInst);
        CmpInst* compareInst1 = CmpInst::Create(Instruction::ICmp, CmpInst::ICMP_UGT, int64CastInst, upperCastInst, "", oldBlock1);
        //int64CastInst1->insertBefore(compareInst1);
        upperCastInst->insertBefore(compareInst1);
        BranchInst* brInst1 = BranchInst::Create(escapeBlock, newBlock, compareInst1, oldBlock1);
        modified = true;
      }
#endif CYCLE_GUARD
      for(auto& F : M){
        //errs() << "XAN: AFTER\n";
        //F.print(errs());
        //                 F.print(errs(), nullptr, false, true);
      }

      //Print results
      errs() << "Guard Information\n";
      errs() << "Unoptimized Guards:\t" << nonOptimizedGuard << "\n"; 
      errs() << "Redundant Optimized Guards:\t" << redundantGuard << "\n"; 
      errs() << "Loop Invariant Hoisted Guards:\t" << loopInvariantGuard << "\n"; 
      errs() << "Scalar Evolution Combined Guards:\t" << scalarEvolutionGuard << "\n"; 
      errs() << "Hoisted Call Guards\t" << callGuardOpt << "\n"; 
      errs() << "Total Guards:\t" << nonOptimizedGuard + loopInvariantGuard + scalarEvolutionGuard << "\n"; 

    }

    void getAnalysisUsage (AnalysisUsage &AU) const override {
      AU.addRequired<LoopInfoWrapperPass>();
      AU.addRequired<ScalarEvolutionWrapperPass>();
      AU.addRequired<Noelle>();
      return ;
    }
  };


  // Next there is code to register your pass to "opt"
  char PROTECT::ID = 0;
  static RegisterPass<PROTECT> X("CARATprotector", "Bounds protection for CARAT");

  // Next there is code to register your pass to "clang"
  static PROTECT * _PassMaker = NULL;
  static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
      [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
      if(!_PassMaker){ PM.add(_PassMaker = new PROTECT());}}); // ** for -Ox
  static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
      [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
      if(!_PassMaker){ PM.add(_PassMaker = new PROTECT());}}); // ** for -O0




} //End namespace
