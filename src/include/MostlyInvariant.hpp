#pragma once

#ifndef MOSTLY_INVARIANT
#define MOSTLY_INVARIANT

#include <unordered_set>
#include <unordered_map>
#include <map>

#include "Utils.hpp"

using namespace llvm::noelle;

std::unordered_set<Instruction*> getLoopMostlyInvariants(Noelle &noelle, std::unordered_set<Instruction *> &instsToSkip, std::unordered_map<Instruction*,Entry*> &instToEntryMap);
bool instrumentLoopMostlyInvariants(Module &M, std::map<Function *, Instruction *> &funcToCallstack, std::unordered_map<Instruction *, Entry *> &instToEntryMap, std::unordered_set<Instruction *> insts);

#endif
