#pragma once

#ifndef AVOID_LOCAL_VARIABLE_INSTRUMENTATION
#define AVOID_LOCAL_VARIABLE_INSTRUMENTATION

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"

#include <unordered_set>

using namespace llvm;

std::unordered_set<Function*> findFunctionsToInstrument(Module &M);

#endif
