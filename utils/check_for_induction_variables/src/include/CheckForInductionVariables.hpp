#pragma once

#ifndef CHECK_FOR_INDUCTION_VARIABLES
#define CHECK_FOR_INDUCTION_VARIABLES

#include "../srcCARMOT/include/Utils.hpp"

bool checkForInductionVariable(Noelle &noelle, Instruction *roiInst, StayConnectedNestedLoopForestNode *L);
std::vector<Instruction*> checkForInductionVariables(Noelle &noelle, std::unordered_map<Instruction*, StayConnectedNestedLoopForestNode*> &roiToLoopMap);
std::vector<Instruction*> getOriginalROIsToPromoteAllocas(Module &M, std::vector<Instruction*> &roisCloned);

#endif
