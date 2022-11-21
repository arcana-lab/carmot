/*
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2020, Drew Kersnar <drewkersnar2021@u.northwestern.edu>
 * Copyright (c) 2020, Gaurav Chaudhary <gauravchaudhary2021@u.northwestern.edu>
 * Copyright (c) 2020, Souradip Ghosh <sgh@u.northwestern.edu>
 * Copyright (c) 2020, Brian Suchy <briansuchy2022@u.northwestern.edu>
 * Copyright (c) 2020, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2020, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Drew Kersnar, Gaurav Chaudhary, Souradip Ghosh, 
 *          Brian Suchy, Peter Dinda 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include "../include/States.hpp"

StateHandler::StateHandler(Module *M, 
                           std::unordered_map<std::string, int> *FunctionMap,
                           uint64_t tT){
    DEBUG_INFO("--- State Tracking Constructor ---\n");

    // Set state
    this->M = M;
    this->FunctionMap = FunctionMap;
    this->temporalTracking = tT;
    // Set up data structures
    this->MemUses = std::vector<Instruction *>();
    this->mainReturns = *(Utils::GetReturnsFromMain(this->M));

    this->_getAllNecessaryInstructions();
    this->_buildNecessaryFunctions();
}


void StateHandler::_buildNecessaryFunctions(){
    LLVMContext &TheContext = this->M->getContext();

    Type* voidType = Type::getVoidTy(TheContext);
    Type* VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection
    Type* int64Type = Type::getInt64Ty(TheContext);
    //Build the report function
    auto signature = FunctionType::get(voidType,false); 
    auto reportFuncCallee = M->getOrInsertFunction(CARAT_STATE_REPORT, signature);
    Function* reportFunc = M->getFunction(CARAT_STATE_REPORT);
    
    NecessaryMethods.insert(std::make_pair(CARAT_STATE_REPORT, reportFunc));

    //Build the add to state function

    Type* argsStateAddFunc[] = {VoidPointerType,
                               VoidPointerType,
                               int64Type};

    FunctionType* argsStateAddFuncTy = FunctionType::get(voidType, 
                                                  ArrayRef<Type*>(argsStateAddFunc,3), 
                                                  false);
    auto stateAddFuncCallee = M->getOrInsertFunction(CARAT_ADD_STATE, argsStateAddFuncTy);
    Function* stateAddFunc = M->getFunction(CARAT_ADD_STATE);
    NecessaryMethods.insert(std::make_pair(CARAT_ADD_STATE, stateAddFunc));

}


void StateHandler::_getAllNecessaryInstructions()
{


    for (auto &F : *M)
    {
        if ((FunctionMap->find(F.getName()) != FunctionMap->end()) 
                || (isCARATFunc(F.getName())) 
                || (F.isIntrinsic()) 
                || (!(F.getInstructionCount())))
        { continue; }

        for (auto &B : F)
        {
            for (auto &I : B)
            {
                if (isa<StoreInst>(&I))
                {
                    StoreInst *SI = static_cast<StoreInst *>(&I);
                    MemUses.push_back(SI);                                
                }
                else if(isa<LoadInst>(I)){
                    LoadInst* LI = &(cast<LoadInst>(I));
                    MemUses.push_back(LI);
                }
                else if (isa<CallInst>(&I) || isa<InvokeInst>(&I))
                {
                    Function *fp = nullptr;

                    // Make sure it is a library call
                    if (isa<CallInst>(I))
                    {
                        auto *CI = static_cast<CallInst*>(&I);
                        fp = CI->getCalledFunction();
                    }
                    else
                    {
                        auto *II = static_cast<InvokeInst*>(&I);
                        fp = II->getCalledFunction();
                    }

                    //Continue if fails
                    if (fp != nullptr)
                    {
                        if (fp->empty()) // Suspicious
                        {
                            // name is fp->getName();
                            StringRef funcName = fp->getName();

                            DEBUG_INFO(funcName + "\n");

                            int32_t val = (*FunctionMap)[funcName];

                            switch (val)
                            {
                            case NULL: // Did not find the function, error
                            {

                                DEBUG_INFO("STATE: The following function call is external to the program and not accounted for in our map "
                                           + funcName + "\n");

                                break;
                            }
                            //Memmove or memcpy inst
                            case -3:
                            {
                                MemUses.push_back(&I);
                                break;
                            }
                            default: // Terrible
                            {
                                break;
                            }
                            }
                        }
                    }
                }
            }
        }
    }

    return;
}

void StateHandler::Inject()
{
    Function *TEXASAddState = NecessaryMethods[CARAT_ADD_STATE];
    Function *TEXASReportStates = NecessaryMethods[CARAT_STATE_REPORT];
    LLVMContext &TheContext = TEXASAddState->getContext();

    // Set up types necessary for injections
    Type *VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection
    Type* int64Type = Type::getInt64Ty(TheContext);

    for(auto RI : mainReturns){
        IRBuilder<> RIBuilder = Utils::GetBuilder(RI->getFunction(), RI);
        std::vector<Value *> CallArgs;
        ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);
        CallInst *StateReport = RIBuilder.CreateCall(TEXASReportStates, LLVMCallArgs);
    }

    for (auto MU : MemUses)
    {
        Instruction *InsertionPoint = MU->getNextNode();
        if (InsertionPoint == nullptr)
        {
            errs() << "Not able to instrument: ";
            MU->print(errs());
            errs() << "\n";

            continue;
        }

        // Set up injections and call instruction arguments
        IRBuilder<> MUBuilder = Utils::GetBuilder(MU->getFunction(), InsertionPoint);
        std::vector<Value *> CallArgs;

        // Get pointer operand from store instruction --- this is the
        // only parameter technically in the state
        Value* PointerOperand;         
        Value* ValueOperand = ConstantPointerNull::get(PointerType::get(VoidPointerType, 0));
        if(isa<StoreInst>(MU)){ 
            StoreInst *SMU = static_cast<StoreInst *>(MU);
            PointerOperand = SMU->getPointerOperand(); 
            if(SMU->getValueOperand()->getType()->isPointerTy()){
                ValueOperand = SMU->getValueOperand();
            }
        }
        else if(isa<LoadInst>(MU)){ 
            LoadInst *LMU = static_cast<LoadInst *>(MU);
            PointerOperand = LMU->getPointerOperand(); 
        }
        else if (isa<CallInst>(MU))
        {
            CallInst *CI = static_cast<CallInst *>(MU);
            PointerOperand = CI->getOperand(0);
            ValueOperand = CI->getOperand(1);
        }
        else if(isa<InvokeInst>(MU))
        {
            InvokeInst* II = static_cast<InvokeInst *>(MU);
            PointerOperand = II->getOperand(0);
            ValueOperand = II->getOperand(1);
        }

        // Pointer operand casted to void pointer
        Value *PointerOperandCast = MUBuilder.CreatePointerCast(PointerOperand, VoidPointerType);
        Value *ValueOperandCast = MUBuilder.CreatePointerCast(ValueOperand, VoidPointerType);
        Value* dinaInt = ConstantInt::get(int64Type, this->temporalTracking, false);  
        // Add all necessary arguments, convert to LLVM data structure
        CallArgs.push_back(PointerOperandCast);
        CallArgs.push_back(ValueOperandCast);
        CallArgs.push_back(dinaInt);
        ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

        // Build the call instruction to CARAT escape
        CallInst *AddToState = MUBuilder.CreateCall(TEXASAddState, LLVMCallArgs);
    }

    return;
}
