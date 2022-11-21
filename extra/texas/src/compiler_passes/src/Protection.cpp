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

#include "../include/Protection.hpp"

ProtectionsHandler::ProtectionsHandler(Module *M,
                                       std::unordered_map<std::string, int> *FunctionMap)
{
    DEBUG_INFO("--- Protections Constructor ---\n");

    // Set state
    this->M = M;
    this->FunctionMap = FunctionMap;
    this->Panic = M->getFunction("panic");
    if (this->Panic == nullptr) { abort(); }
    
    // Get globals
    this->LowerBound = M->getGlobalVariable(LOWER_BOUND);
    if (this->LowerBound == nullptr) { abort(); }

    this->UpperBound = M->getGlobalVariable(UPPER_BOUND);
    if (this->UpperBound == nullptr) { abort(); }

    this->PanicString = M->getGlobalVariable(PANIC_STRING);
    if (this->PanicString == nullptr) { abort(); }

    // Set up data structures
    this->MemoryInstructions = std::unordered_map<Instruction *, pair<Instruction *, Value *>>();
    this->EscapeBlocks = std::unordered_map<Function *, BasicBlock *>();
    this->BoundsLoadInsts = std::unordered_map<Function *, pair<LoadInst *, LoadInst *> *>();

    // Do analysis
    this->_getAllNecessaryInstructions();
}

void ProtectionsHandler::_getAllNecessaryInstructions()
{
    return;
}

BasicBlock *ProtectionsHandler::_buildEscapeBlock(Function *F)
{
    /* 

       Escape block structure:

       ...
       ...

       escape: 
        %l = load PanicString
        %p = tail call panic(PanicString)
        %u = unreachable

    */

    // Set up builder
    IRBuilder<> EscapeBuilder = Utils::GetBuilder(F, EscapeBlock);

    // Create escape basic block, save the state
    EscapeBlock = BasicBlock::Create(F->getContext(), "escape", F);
    EscapeBlocks[F] = EscapeBlock;

    // Load the panic string global
    LoadInst *PanicStringLoad = EscapeBuilder.CreateLoad(PanicString);

    // Set up arguments for call to panic
    std::vector<Value *> CallArgs;
    CallArgs.push_back(PanicStringLoad);
    ArrayRef<Value *> LLVMCallArgs = ArrayRef<Value *>(CallArgs);

    // Build call to panic
    CallInst *PanicCall = EscapeBuilder.CreateCall(Panic, LLVMCallArgs)
    
    // Add LLVM unreachable
    UnreachableInst *Unreachable = EscapeBuilder.CreateUnreachable();

    return EscapeBlock;
}

pair<LoadInst *, LoadInst *> *ProtectionsHandler::_buildBoundsLoadInsts(Function *F)
{
    /* 

       Bounds loads structure:

       entry: 
        %l = load lower_bound
        %u = load upper_bound

        ...

        %0

    */

    // Get the insertion point
    BasicBlock *EntryBlock = &(F->getEntryBlock());
    Instruction *InsertionPoint = EntryBlock->getFirstNonPHI();
    if (InsertionPoint == nullptr)
        abort(); // Serious

    // Set up the builder
    IRBuilder<> BoundsBuilder = Utils::GetBuilder(F, InsertionPoint);

    // Insert lower bound, then upper bound
    LoadInst *LowerBoundLoad = BoundsBuilder.CreateLoad(LowerBound);
    LoadInst *UpperBoundLoad = BoundsBuilder.CreateLoad(UpperBound);

    // Build pair object, save the state
    pair<LoadInst *, LoadInst *> *NewBoundsPair = new pair<LoadInst *, LoadInst *>();
    NewBoundsPair->first = LowerBoundLoad;
    NewBoundsPair->second = UpperBoundLoad;
    BoundsLoadInsts[F] = NewBoundsPair;
    
    return NewBoundsPair;
}

void ProtectionsHandler::Inject()
{
    for (auto const &[MI, InjPair] : MemoryInstructions)
    {
        // Break down the pair object
        Instruction *InjectionLocation = InjPair.first;
        Value *AddressToCheck = InjPair.second;

        // Get function to inject in
        Function *ParentFunction = I->getFunction();

        // Make sure the escape block exists in the parent
        // function --- or else, build it
        BasicBlock *EscapeBlock = (EscapeBlocks.find(ParentFunction) == EscapeBlocks.end()) ?
                                   _buildEscapeBlock(ParentFunction) :
                                   EscapeBlocks[ParentFunction];

        // Make sure the lower/upper bounds load instructions
        // are injected in the entry block --- if not, build them
        pair<LoadInst *, 
             LoadInst *> *BoundsLoads = (BoundsLoadInsts.find(ParentFunction) == BoundsLoadInsts.end()) ?
                                         _buildBoundsLoadInsts(ParentFunction) :
                                         BoundsLoadInsts[ParentFunction];

        // Break down the bounds load instructions
        LoadInst *LowerBoundLoad = BoundsLoads->first;
        LoadInst *UpperBoundLoad = BoundsLoads->second;

        // Optimized injections (implemented) vs. original 
        // CARAT injections
        /*
            %i = InjectionLocation
            
            --- OPTIMIZED ---
            ---
            
            entry-block:
              %l = load lower_bound
              %u = load upper_bound

            ...
            ...

            optimized-injection:
              %0 = icmp addressToCheck %l
              %1 = icmp addressToCheck %u
              %2 = and %0, %1
              %3 = br %2 %original %escape

            original:
              %i = InjectionLocation

            ...
            ...

            escape:
              %lp = load PanicString
              %escape1 = panic ...
              unreachable
            
            ---
            --- OLD ---
            ---

            carat-original-injection:
              %0 = load lower_bound
              %1 = icmp addressToCheck %0
              %2 = br %0 %continue-block %escape

            carat-continue-block:
              %3 = load upper_bound
              %4 = icmp addressToCheck %3
              %5 = br %3 %original %escape

            original:
              %i = InjectionLocation

            ...
            ...
            
            escape:
              %lp = load PanicString
              %escape1 = panic ...
              unreachable

            ---
        */

        /*
            Split the block --- 

            1) Inject icmp + and instructions at insertion point

            2) Split the block --- SplitBlock will patch an unconditional
               into OldBlock --- from OldBlock to NewBlock

            3) Modify the unconditional branch instruction to reflect
               a conditional jump to escape if necessary
        */
    
        // Set up builder
        IRBuilder<> ChecksBuilder = Utils::GetBuilder(F, InjectionLocation);

        // Inject compare for lower bound, then compare for upper bound,
        // then and instruction with both compares as operands
        Value *LowerBoundCompare = ChecksBuilder.CreateICmpULT(AddressToCheck, LowerBoundLoad),
              *UpperBoundCompare = ChecksBuilder.CreateICmpUGT(AddressToCheck, UpperBoundLoad),
              *AndInst = ChecksBuilder.CreateAnd(LowerBoundCompare, UpperBoundCompare);

        // Now split the block
        BasicBlock *OldBlock = InjectionLocation->getParent();
        BasicBlock *NewBlock = SplitBlock(OldBlock, InjectionLocation);
        
        // Reconfigure the OldBlock's unconditional branch to handle
        // conditional jump to escape block --- use the AndInst as 
        // the condition
        Instruction *OldBlockTerminator = OldBlock->getTerminator();
        BranchInst *BranchToModify = dyn_cast<BranchInst>(OldBlockTerminator);
        if (BranchToModify == nullptr)
            abort();

        BranchToModify->setCondition(AndInst);
        BranchToModify->setSuccessor(1, EscapeBlock); // 0th successor is NewBlock
    }

    return;
}