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

#include "../include/Allocation.hpp"

AllocationHandler::AllocationHandler(Module *M, 
        std::unordered_map<std::string, int> *FunctionMap)
{
    DEBUG_INFO("--- Allocation Constructor ---\n");

    // Set state
    this->M = M;
    this->FunctionMap = FunctionMap;
    this->Main = M->getFunction("main");
    if (this->Main == nullptr) { abort(); }

    // Set up data structures
    this->Globals = std::unordered_map<GlobalValue *, uint64_t>();
    this->Mallocs = std::vector<Instruction *>();
    this->Callocs = std::vector<Instruction *>();
    this->Reallocs = std::vector<Instruction *>();
    this->Frees = std::vector<Instruction *>();
    this->Allocas = std::vector<Instruction *>();
    this->Returns = *(Utils::GetReturnsFromMain(this->M));

    this->_buildNecessaryFunctions();
    this->_getAllNecessaryInstructions();
    this->_getAllGlobals();
}

void AllocationHandler::_buildNecessaryFunctions(){
    // CARAT_MALLOC = "_Z20AddToAllocationTablePvm",
    // CARAT_REALLOC = "_Z30HandleReallocInAllocationTablePvS_m",
    // CARAT_CALLOC = "_Z26AddCallocToAllocationTablePvmm",
    // CARAT_REMOVE_ALLOC = "_Z25RemoveFromAllocationTablePv",
    // CARAT_STATS = "_Z16ReportStatisticsv" 

    LLVMContext &TheContext = this->M->getContext();

    Type* voidType = Type::getVoidTy(TheContext);
    Type* VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection
    Type* int64Type = Type::getInt64Ty(TheContext);
    //Build the report function
    auto signature = FunctionType::get(voidType,false); 
    auto reportFuncCallee = M->getOrInsertFunction(CARAT_STATS, signature);
    Function* reportFunc = M->getFunction(CARAT_STATS);
    NecessaryMethods.insert(std::make_pair(CARAT_STATS, reportFunc));

    //Build the add MALLOC 
    errs() << "Building add malloc\n";

    Type* argsMallocAddFunc[] = {VoidPointerType,
        int64Type};
    FunctionType* argsMallocAddFuncTy = FunctionType::get(voidType, 
            ArrayRef<Type*>(argsMallocAddFunc,2), 
            false);
    auto AddMallocCallee = M->getOrInsertFunction(CARAT_MALLOC, argsMallocAddFuncTy);
    Function* MallocFunc = M->getFunction(CARAT_MALLOC);
    NecessaryMethods.insert(std::make_pair(CARAT_MALLOC, MallocFunc));

    //Build the add REALLOC 
    errs() << "Building add realloc\n";
    Type* argsReallocAddFunc[] = {VoidPointerType,
        VoidPointerType,
        int64Type};
    FunctionType* argsReallocAddFuncTy = FunctionType::get(voidType, 
            ArrayRef<Type*>(argsReallocAddFunc,3), 
            false);
    auto AddReallocCallee = M->getOrInsertFunction(CARAT_REALLOC, argsReallocAddFuncTy);
    Function* ReallocFunc = M->getFunction(CARAT_REALLOC);
    NecessaryMethods.insert(std::make_pair(CARAT_REALLOC, ReallocFunc));
    
    //Build the add CALLOC 
    errs() << "Building add calloc\n";
    Type* argsCallocAddFunc[] = {VoidPointerType,
        int64Type,
        int64Type};
    FunctionType* argsCallocAddFuncTy = FunctionType::get(voidType, 
            ArrayRef<Type*>(argsReallocAddFunc,3), 
            false);
    auto AddCallocCallee = M->getOrInsertFunction(CARAT_CALLOC, argsCallocAddFuncTy);
    Function* CallocFunc = M->getFunction(CARAT_CALLOC);
    NecessaryMethods.insert(std::make_pair(CARAT_CALLOC, CallocFunc));

    //Build the add RemoveFromAllocTable 
    errs() << "Building add free\n";
    Type* argsRFATAddFunc[] = {VoidPointerType};
    FunctionType* argsRFATAddFuncTy = FunctionType::get(voidType, 
            ArrayRef<Type*>(argsRFATAddFunc,1), 
            false);
    auto AddRFATCallee = M->getOrInsertFunction(CARAT_REMOVE_ALLOC, argsRFATAddFuncTy);
    Function* RFATFunc = M->getFunction(CARAT_REMOVE_ALLOC);
    NecessaryMethods.insert(std::make_pair(CARAT_REMOVE_ALLOC, RFATFunc));



}


void AllocationHandler::Inject()
{
    AddAllocationTableCallToMain();
    InjectMallocCalls();
    InjectCallocCalls();
    InjectReallocCalls();
    InjectFreeCalls();
    InjectReportCalls();

    return;
}

void AllocationHandler::_getAllNecessaryInstructions()
{
    // This triple nested loop will just go through all the 
    // instructions and sort all the allocations into their 
    // respective types.
    for (auto &F : *M)
    {
        if ((FunctionMap->find(F.getName()) != FunctionMap->end()) 
                || (isCARATFunc(F.getName())) 
                || (F.isIntrinsic()) 
                || (!(F.getInstructionCount())))
        { continue; }

        DEBUG_INFO("Entering function " + F.getName() + "\n");

        // This will stop us from iterating through printf and malloc and stuff.
        for (auto &B : F)
        {
            for (auto &I : B)
            {
                DEBUG_INFO("Working on following instruction: ");
                OBJ_INFO((&I));

                if (isa<AllocaInst>(I))
                {
                    Allocas.push_back(&I);
                }

                // First we will check to see if the given instruction is a free instruction or a malloc.
                // This requires that we first check to see if the instruction is a call instruction.
                // Then we check to see if the call is not present (implying it is a library call).
                // If it is present, next; otherwise, we check if it is free(). Next we see if it 
                // is an allocation within a database of allocations (malloc, calloc, realloc, jemalloc ...)
                if (isa<CallInst>(I) || isa<InvokeInst>(I))
                {
                    Function *fp = nullptr;

                    // Make sure it is a library call
                    if (isa<CallInst>(I))
                    {
                        CallInst *CI = &(cast<CallInst>(I));
                        fp = CI->getCalledFunction();
                    }
                    else
                    {
                        InvokeInst *II = &(cast<InvokeInst>(I));
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
                                        /*

                                           if (FunctionsToAvoid.find(funcName) == FunctionsToAvoid.end())
                                           {
                                           errs() << "The following function call is external to the program and not accounted for in our map " << funcName << "\n";
                                           FunctionsToAvoid.insert(funcName);
                                           }

                                         */

                                        DEBUG_INFO("The following function call is external to the program and not accounted for in our map "
                                                + funcName + "\n");

                                        // Maybe it would be nice to add a prompt asking if the function is an allocation, free, or meaningless for our program instead of just dying
                                        // Also should the program maybe save the functions in a saved file (like a json/protobuf) so it can share knowledge between runs.
                                        // If we go the prompt route then we should change the below statements to simply if statements.

                                        break;
                                    }
                                case 2: // Function is an allocation instruction
                                    {
                                        Mallocs.push_back(&I);
                                        break;
                                    }
                                case 3: // Function has the signature of calloc
                                    {
                                        Callocs.push_back(&I);
                                        break;
                                    }
                                case 4: // Function has the signature of realloc
                                    {
                                        Reallocs.push_back(&I);
                                        break;
                                    }
                                case 1: //Function is a deallocation instuction
                                    {
                                        Frees.push_back(&I);
                                        break;
                                    }
                                case -1:
                                    {
                                        DEBUG_INFO("The following function call is external to the program, but the signature of the allocation is not supported (...yet)" + funcName + "\n");
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

// NEEDS MAJOR REFACTORING
void AllocationHandler::_getAllGlobals()
{
    // This will go through the current global variables and make sure
    // we are covering all heap allocations
    for (auto &Global : M->getGlobalList())
    {
        // Cannot target LLVM-specific globals
        if ((Global.getName() == "llvm.global_ctors") 
                || (Global.getName() == "llvm.used"))
        { continue; }

        uint64_t totalSizeInBytes;

        // Each global variable can be either a struct, array, or a primitive.
        // We will figure that out to calculate the total size of the global.
        if (Global.getType()->isPointerTy())
        {
            // errs() << "Working with global: " << Global << "That is a pointer of type: " << *(Global.getValueType()) << "\n";
            Type *iterType = Global.getValueType();
            if (iterType->isArrayTy())
            {
                totalSizeInBytes = findArraySize(iterType);
            }
            // Now get element size per
            else if (iterType->isStructTy())
            {
                totalSizeInBytes = findStructSize(iterType);
            }
            // We are worried about bytes not bits
            else
            {
                totalSizeInBytes = iterType->getPrimitiveSizeInBits() / 8;
            }
            DEBUG_INFO("The size of the element is: " + to_string(totalSizeInBytes) + "\n");
            Globals[&(cast<GlobalValue>(Global))] = totalSizeInBytes;
        }
    }

    return;
}

// For globals into main
void AllocationHandler::AddAllocationTableCallToMain()
{
    // Set up for IRBuilder, malloc injection
    Instruction *InsertionPoint = Main->getEntryBlock().getFirstNonPHI();
    IRBuilder<> MainBuilder = Utils::GetBuilder(Main, InsertionPoint);

    LLVMContext &TheContext = Main->getContext();
    Type *VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection
    Function *CARATMalloc = NecessaryMethods[CARAT_MALLOC];

    for (auto const &[GV, Length] : Globals)
    {
        // Set up arguments for call instruction to malloc
        std::vector<Value *> CallArgs;

        // Build void pointer cast for global
        Value *PointerCast = MainBuilder.CreatePointerCast(GV, VoidPointerType);

        // Add to arguments vector
        CallArgs.push_back(PointerCast);
        CallArgs.push_back(ConstantInt::get(IntegerType::get(TheContext, 64), Length, false));

        // Convert to LLVM data structure
        ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

        // Build call instruction
        CallInst *MallocInjection = MainBuilder.CreateCall(CARATMalloc, LLVMCallArgs);
    }

    return;
}

void AllocationHandler::InjectMallocCalls()
{
    Function *CARATMalloc = NecessaryMethods[CARAT_MALLOC];
    LLVMContext &TheContext = CARATMalloc->getContext();

    // Set up types necessary for injections
    Type *VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection
    Type *Int64PtrType = Type::getInt64PtrTy(TheContext, 0);      // For pointer injection
    Type* Int64Type = Type::getInt64Ty(TheContext);
    for (auto MI : Mallocs)
    {
        Instruction *InsertionPoint = MI->getNextNode();
        if (InsertionPoint == nullptr)
        {
            errs() << "Not able to instrument: ";
            MI->print(errs());
            errs() << "\n";

            continue;
        }

        // Set up injections and call instruction arguments
        IRBuilder<> MIBuilder = Utils::GetBuilder(MI->getFunction(), InsertionPoint);
        std::vector<Value *> CallArgs;

        // Cast inst as value to grab returned value
        Value *MallocReturnCast = MIBuilder.CreatePointerCast(MI, VoidPointerType);

        // Cast inst for size argument to original malloc call (MI)
        Value *MallocSizeArgCast = MIBuilder.CreateZExtOrBitCast(MI->getOperand(0), Int64Type);

        // Add CARAT malloc call instruction arguments
        CallArgs.push_back(MallocReturnCast);
        CallArgs.push_back(MallocSizeArgCast);
        ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

        // Build the call instruction to CARAT malloc
        CallInst *AddToAllocationTable = MIBuilder.CreateCall(CARATMalloc, LLVMCallArgs);
    }

    return;
}

void AllocationHandler::InjectCallocCalls()
{
    Function *CARATCalloc = NecessaryMethods[CARAT_CALLOC];
    LLVMContext &TheContext = CARATCalloc->getContext();

    // Set up types necessary for injections
    Type *VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection
    Type* Int64Type = Type::getInt64Ty(TheContext);

    for (auto CI : Callocs)
    {
        Instruction *InsertionPoint = CI->getNextNode();
        if (InsertionPoint == nullptr)
        {
            errs() << "Not able to instrument: ";
            CI->print(errs());
            errs() << "\n";

            continue;
        }

        // Set up injections and call instruction arguments
        IRBuilder<> CIBuilder = Utils::GetBuilder(CI->getFunction(), InsertionPoint);
        std::vector<Value *> CallArgs;

        // Cast inst as value to grab returned value
        Value *CallocReturnCast = CIBuilder.CreatePointerCast(CI, VoidPointerType);

        // Cast inst for size argument to original calloc call (CI)
        Value *CallocSizeArgCast = CIBuilder.CreateZExtOrBitCast(CI->getOperand(0), Int64Type);

        // Cast inst for second argument (number of elements) to original calloc call (CI)
        Value *CallocNumElmCast = CIBuilder.CreateZExtOrBitCast(CI->getOperand(1), Int64Type);

        // Add CARAT calloc call instruction arguments
        CallArgs.push_back(CallocReturnCast);
        CallArgs.push_back(CallocSizeArgCast);
        CallArgs.push_back(CallocNumElmCast);
        ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

        // Build the call instruction to CARAT calloc
        CallInst *AddToAllocationTable = CIBuilder.CreateCall(CARATCalloc, LLVMCallArgs);
    }

    return;
}

void AllocationHandler::InjectReallocCalls()
{
    Function *CARATRealloc = NecessaryMethods[CARAT_REALLOC];
    LLVMContext &TheContext = CARATRealloc->getContext();

    // Set up types necessary for injections
    Type *VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection
    Type* Int64Type = Type::getInt64Ty(TheContext);

    for (auto RI : Reallocs)
    {
        Instruction *InsertionPoint = RI->getNextNode();
        if (InsertionPoint == nullptr)
        {
            errs() << "Not able to instrument: ";
            RI->print(errs());
            errs() << "\n";

            continue;
        }

        // Set up injections and call instruction arguments
        IRBuilder<> RIBuilder = Utils::GetBuilder(RI->getFunction(), InsertionPoint);
        std::vector<Value *> CallArgs;

        // Cast inst for the old pointer passed to realloc
        Value *ReallocOldPtrCast = RIBuilder.CreatePointerCast(RI->getOperand(0), VoidPointerType);

        // Cast inst for return value from original Realloc call (RI)
        Value *ReallocReturnCast = RIBuilder.CreateZExtOrBitCast(RI, VoidPointerType);

        // Cast inst for size argument to original Realloc call (RI)
        Value *ReallocNewSizeCast = RIBuilder.CreateZExtOrBitCast(RI->getOperand(1), Int64Type);

        // Add CARAT Realloc call instruction arguments
        CallArgs.push_back(ReallocOldPtrCast);
        CallArgs.push_back(ReallocReturnCast);
        CallArgs.push_back(ReallocNewSizeCast);
        ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

        // Build the call instruction to CARAT Realloc
        CallInst *AddToAllocationTable = RIBuilder.CreateCall(CARATRealloc, LLVMCallArgs);
    }

    return;
}

void AllocationHandler::InjectReportCalls()
{
    Function *CARATStats = NecessaryMethods[CARAT_STATS];
    LLVMContext &TheContext = CARATStats->getContext();

    // Set up types necessary for injections
    Type *VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection

    for (auto RI : Returns)
    {

        // Set up injections and call instruction arguments
        IRBuilder<> RIBuilder = Utils::GetBuilder(RI->getFunction(), RI);
        // Build the call instruction to CARAT Free
        CallInst *CaratStatsCall = RIBuilder.CreateCall(CARATStats);
    }

    return;

}

void AllocationHandler::InjectFreeCalls()
{
    Function *CARATFree = NecessaryMethods[CARAT_REMOVE_ALLOC];
    LLVMContext &TheContext = CARATFree->getContext();

    // Set up types necessary for injections
    Type *VoidPointerType = Type::getInt8PtrTy(TheContext, 0); // For pointer injection

    for (auto FI : Frees)
    {

        // Set up injections and call instruction arguments
        IRBuilder<> FIBuilder = Utils::GetBuilder(FI->getFunction(), FI);
        std::vector<Value *> CallArgs;

        // Cast inst as value passed to free
        Value *FreePassedPtrCast = FIBuilder.CreatePointerCast(FI->getOperand(0), VoidPointerType);

        // Add CARAT Free call instruction arguments
        CallArgs.push_back(FreePassedPtrCast);
        ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

        // Build the call instruction to CARAT Free
        CallInst *AddToAllocationTable = FIBuilder.CreateCall(CARATFree, LLVMCallArgs);
    }

    return;
}
