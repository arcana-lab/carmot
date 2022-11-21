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

#include "../include/Utils.hpp"

using namespace Utils;
using namespace Debug;

/*
 * ExitOnInit
 * 
 * Register pass, execute doInitialization method but do not perform
 * any analysis or transformation --- exit in runOnModule --- mostly
 * for testing scenarios
 * 
 */

void Utils::ExitOnInit()
{
    if (FALSE)
    {
        errs() << "Exiting KARAT Transforms ...\n";
        exit(0);
    }
}


/*
 * GetBuilder
 * 
 * Generates a specific IRBuilder instance that is fitted with 
 * the correct debug location --- necessary for injections 
 * into the Nautilus bitcode
 * 
 */

IRBuilder<> Utils::GetBuilder(Function *F, Instruction *InsertionPoint)
{
    IRBuilder<> Builder{InsertionPoint};
    Instruction *FirstInstWithDBG = nullptr;

    for (auto &I : instructions(F))
    {
        if (I.getDebugLoc())
        {
            FirstInstWithDBG = &I;
            break;
        }
    }

    if (FirstInstWithDBG != nullptr)
        Builder.SetCurrentDebugLocation(FirstInstWithDBG->getDebugLoc());

    return Builder;
}

std::vector<Instruction *>* Utils::GetReturnsFromMain(Module* M){
    Function* main = M->getFunction("main");
    if(!main){
        return nullptr;
    }
    std::vector<Instruction *>* returns = new std::vector<Instruction *>();
    for (auto &I : instructions(main)){
        if(isa<ReturnInst>(I)){
            returns->push_back(&I);
        }  
    }

    return returns;

}

IRBuilder<> Utils::GetBuilder(Function *F, BasicBlock *InsertionPoint)
{
    IRBuilder<> Builder{InsertionPoint};
    Instruction *FirstInstWithDBG = nullptr;

    for (auto &I : instructions(F))
    {
        if (I.getDebugLoc())
        {
            FirstInstWithDBG = &I;
            break;
        }
    }

    if (FirstInstWithDBG != nullptr)
        Builder.SetCurrentDebugLocation(FirstInstWithDBG->getDebugLoc());

    return Builder;
}
