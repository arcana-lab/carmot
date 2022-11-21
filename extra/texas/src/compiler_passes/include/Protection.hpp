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

#include "Profiler.hpp"

using namespace llvm;

class ProtectionsHandler
{
public:
    ProtectionsHandler(Module *M,
                       std::unordered_map<std::string, int> *FunctionMap);

    // Injection methods
    void Inject();

private:
    // Initial state
    Module *M;
    Function *Panic;
    GlobalVariable *PanicString;
    GlobalVariable *LowerBound;
    GlobalVariable *UpperBound;

    // Analyzed state
    std::unordered_map<Instruction *, pair<Instruction *, Value *>> MemoryInstructions;
    std::unordered_map<Function *, BasicBlock *> EscapeBlocks;
    std::unordered_map<Function *, pair<LoadInst *, LoadInst *> *> BoundsLoadInsts;

    // Private methods
    void _getAllNecessaryInstructions();
    BasicBlock *_buildEscapeBlock(Function *F);
    pair<LoadInst *, LoadInst *> *_buildBoundsLoadInsts(Function *F);
};
