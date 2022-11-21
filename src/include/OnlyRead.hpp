#pragma once

#ifndef ONLY_READ
#define ONLY_READ

#include "Utils.hpp"
#include "ROI.hpp"

using namespace llvm::noelle ;

std::unordered_set<Instruction*> getOnlyReadLoadsAll(Noelle &noelle, std::vector<ROI*> &rois, std::unordered_set<Instruction*> &instsToSkip, std::unordered_map<Instruction*, Entry*> &instToEntryMap);
bool instrumentOnlyReads(Module &M, std::map<Function *, Instruction *> &funcToCallstack, std::unordered_map<Instruction *, Entry *> &instToEntryMap, std::unordered_set<Instruction*> &instsToInstrument);

#endif
