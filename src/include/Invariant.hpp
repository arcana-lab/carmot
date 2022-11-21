#pragma once

#ifndef INVARIANT
#define INVARIANT

#include "Utils.hpp"

using namespace llvm::noelle ;

std::unordered_set<Instruction*> getLoopInvariants(Module &M, Noelle &noelle, std::unordered_set<Instruction*> &instsToSkip, std::unordered_map<Instruction*, Entry*> &instToEntryMap);
bool instrumentLoopInvariants(Module &M, std::map<Function *, Instruction *> &funcToCallstack, std::unordered_map<Instruction *, Entry *> &instToEntryMap, std::unordered_set<Instruction*> &instsToInstrument);

#endif
