#pragma once

#ifndef PROMOTE_ALLOCA_TO_REG
#define PROMOTE_ALLOCA_TO_REG

#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include "../srcCARMOT/include/Utils.hpp"

bool promoteSingleAlloca(AllocaInst *AI, DominatorTree &DT, AssumptionCache &AC);
std::unordered_map<Function*, std::unordered_map<Instruction*, StayConnectedNestedLoopForestNode*>> getFunctionToLoopsMap(Module &M, Noelle &noelle);
bool promoteAllocasROIs(std::unordered_map<Instruction*, StayConnectedNestedLoopForestNode*> &roiToLoopMap, DominatorTree &DT, AssumptionCache &AC);

#endif
