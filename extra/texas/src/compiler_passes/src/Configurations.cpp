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
 *-
 * Copyright (c) 2020, Brian Suchy <briansuchy2022@u.northwestern.edu>
 * Copyright (c) 2020, Souradip Ghosh <sgh@u.northwestern.edu>
 * Copyright (c) 2020, Peter Dinda <pdinda@northwestern.edu>

 * All rights reserved.
 *
 * Authors: Drew Kersnar, Gaurav Chaudhary, Souradip Ghosh, 
 *          Brian Suchy, Peter Dinda 
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include "../include/Configurations.hpp"




// Function names to inject
const string CARAT_MALLOC = "_Z20AddToAllocationTablePvm",
             CARAT_REALLOC = "_Z30HandleReallocInAllocationTablePvS_m",
             CARAT_CALLOC = "_Z26AddCallocToAllocationTablePvmm",
             CARAT_REMOVE_ALLOC = "_Z25RemoveFromAllocationTablePv",
             CARAT_STATS = "_Z16ReportStatisticsv",
             //Escape Tracking
             CARAT_ESCAPE = "_Z16AddToEscapeTablePv",
             //State Tracking
             CARAT_STATE_REPORT = "_Z12ReportStatesv",
             CARAT_ADD_STATE = "_Z15AddToStateMultiPvS_m",
             //Protection
             PANIC_STRING = "LLVM_panic_string",
             LOWER_BOUND = "lower_bound",
             UPPER_BOUND = "upper_bound";

// Important/necessary methods/method names to track
unordered_map<string, Function *> NecessaryMethods = unordered_map<string, Function *>();
const vector<string> ImportantMethodNames = {CARAT_MALLOC, 
                                             CARAT_REALLOC, 
                                             CARAT_CALLOC,
                                             CARAT_REMOVE_ALLOC, 
                                             CARAT_STATS,
                                             CARAT_ESCAPE,
                                             CARAT_STATE_REPORT,
                                             CARAT_ADD_STATE,
                                             };


bool isCARATFunc(std::string str){
    for(auto a : ImportantMethodNames){
        if(a.compare(str) == 0){
            return true;
        }
    }
    return false;
}



// Other methods --- FIX --- NEED TO REFACTOR
uint64_t findStructSize(Type *sType)
{
    uint64_t size = 0;
    for (int i = 0; i < sType->getStructNumElements(); i++)
    {
        if (sType->getStructElementType(i)->isArrayTy())
        {
            size = size + findArraySize(sType->getStructElementType(i));
        }
        else if (sType->getStructElementType(i)->isStructTy())
        {
            size = size + findStructSize(sType->getStructElementType(i));
        }
        else if (sType->getStructElementType(i)->getPrimitiveSizeInBits() > 0)
        {
            size = size + (sType->getStructElementType(i)->getPrimitiveSizeInBits() / 8);
        }
        else if (sType->getStructElementType(i)->isPointerTy())
        {
            //This is bad practice to just assume 64-bit system... but whatever
            size = size + 8;
        }
        else
        {
            errs() << "Error(Struct): Cannot determine size:" << *(sType->getStructElementType(i)) << "\n";
            return 0;
        }
    }

    DEBUG_INFO("Returning: " + to_string(size) + "\n");

    return size;
}

uint64_t findArraySize(Type *aType)
{
    Type *insideType;
    uint64_t size = aType->getArrayNumElements();

    DEBUG_INFO("Num elements in array: " + to_string(size) + "\n");

    insideType = aType->getArrayElementType();
    if (insideType->isArrayTy())
    {
        size = size * findArraySize(insideType);
    }
    else if (insideType->isStructTy())
    {
        size = size * findStructSize(insideType);
    }
    else if (insideType->getPrimitiveSizeInBits() > 0)
    {
        size = size * (insideType->getPrimitiveSizeInBits() / 8);
    }
    else if (insideType->isPointerTy())
    {
        size = size + 8;
    }
    else
    {
        errs() << "Error(Array): cannot determing size: " << *insideType << "\n";
        return 0;
    }

    DEBUG_INFO("Returning: " + to_string(size) + "\n");

    return size;
}

void populateLibCallMap(std::unordered_map<std::string, int> *functionCalls)
{
    std::pair<std::string, int> call;

    //Allocations
    call.first = "malloc";
    call.second = 2;
    functionCalls->insert(call);

    call.first = "calloc";
    call.second = 3;
    functionCalls->insert(call);

    call.first = "realloc";
    call.second = 4;
    functionCalls->insert(call);

    call.first = "jemalloc";
    call.second = -1;
    functionCalls->insert(call);

    call.first = "mmap";
    call.second = -1;
    functionCalls->insert(call);

    //Mem Uses
    //TODO Ask if memset is important...
    call.first = "llvm.memset.p0i8.i64";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "llvm.memcpy.p0i8.p0i8.i64";
    call.second = -3;
    functionCalls->insert(call);

    call.first = "llvm.memmove.p0i8.p0i8.i64";
    call.second = -3;
    functionCalls->insert(call);

    //Frees
    call.first = "free";
    call.second = 1;
    functionCalls->insert(call);

    //Unimportant calls
    call.first = "printf";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "lrand48";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "fwrite";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "fputc";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "fprintf";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "strtol";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "log";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "exit";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "fread";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "feof";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "fclose";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "fflush";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "strcpy";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "srand48";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "ferror";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "fopen";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "llvm.dbg.value";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "llvm.dbg.declare";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "llvm.lifetime.end.p0i8";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "llvm.lifetime.start.p0i8";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "sin";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "cos";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "tan";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "llvm.fabs.f64";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "timer_read";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "timer_stop";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "timer_start";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "timer_clear";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "randlc";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "vranlc";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "c_print_results";
    call.second = -2;
    functionCalls->insert(call);

    call.first = "puts";
    call.second = -2;
    functionCalls->insert(call);
}
