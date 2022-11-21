#pragma once

#ifndef CLUSTER_CALLSTACK_INSTRUMENTATION
#define CLUSTER_CALLSTACK_INSTRUMENTATION

#include "Utils.hpp"
#include "noelle/core/Noelle.hpp"

using namespace llvm;

std::map<Function*, Instruction*> callstackInstrumentation(Module &M, std::unordered_set<Function*> &uniqueCallstackFunctions);
std::unordered_set<Function*> getUniqueCallstackFunctions(Noelle &noelle);

#endif
