#pragma once

#ifndef CLONE_FUNCTION
#define CLONE_FUNCTION

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

using namespace llvm;

Function* cloneFunction(Function *func);
bool cloneFunctions(Module &M);

#endif
