#pragma once

#ifndef TOUCH_SAME_OBJECT
#define TOUCH_SAME_OBJECT

#include "noelle/core/Noelle.hpp"
#include "ROI.hpp"

using namespace llvm::noelle;

std::unordered_set<Instruction*> getTouchSameObjectInsts(Module &M, Noelle &noelle, std::unordered_set<Function*> &functionsToRunDFA, std::vector<ROI*> &rois, bool enableDFAObjGranularity);

#endif
