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

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DerivedUser.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/AssumptionCache.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <set>

// Pass settings
#define DEBUG 0 
#define FALSE 0
#define VERIFY 0

// Debug
#define DEBUG_INFO(str) do { if (DEBUG) { errs() << str; } } while (0)
#define OBJ_INFO(obj) do { if (DEBUG) { obj->print(errs()); errs() << "\n"; } } while (0)
#define VERIFY_DEBUG_INFO(str) do { if (VERIFY) { errs() << str; } } while (0)
#define VERIFY_OBJ_INFO(obj) do { if (VERIFY) { obj->print(errs()); errs() << "\n"; } } while (0)

using namespace llvm;
using namespace std;

// Function names to inject
extern const string CARAT_MALLOC;
extern const string CARAT_REALLOC;
extern const string CARAT_CALLOC;
extern const string CARAT_REMOVE_ALLOC;
extern const string CARAT_STATS;
extern const string CARAT_STATE_REPORT;
extern const string CARAT_ADD_STATE;
extern const string CARAT_ESCAPE;
extern const string LOWER_BOUND;
extern const string UPPER_BOUND;

// Important/necessary methods/method names to track
extern unordered_map<string, Function *> NecessaryMethods;
extern const vector<string> ImportantMethodNames;

// Extra utility methods --- FIX --- NEED TO REFACTOR
uint64_t findStructSize(Type *sType);
uint64_t findArraySize(Type *aType);
void populateLibCallMap(std::unordered_map<std::string, int> *functionCalls);
bool isCARATFunc(std::string str);
