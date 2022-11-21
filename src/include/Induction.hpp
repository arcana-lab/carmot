#pragma once

#ifndef INDUCTION
#define INDUCTION

#include "Utils.hpp"
#include "noelle/core/Noelle.hpp"
#include <unordered_set>

bool isConnectedToAnInductionVariable(Value *val, InductionVariableManager *inductionVariableManager);
std::unordered_set<Instruction*> getLoopInductions(Noelle &noelle, std::unordered_set<Instruction *> &instsToSkip, std::unordered_map<Instruction*, Entry*> &instToEntryMap);

#endif
