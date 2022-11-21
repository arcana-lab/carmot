#pragma once

#include "noelle/core/Noelle.hpp"

using namespace llvm::noelle ;

void visit(CallGraphFunctionNode *currentNode, std::unordered_set<Function*> &functions);
std::unordered_set<Function*> getAllFunctions(Module &M);
std::unordered_set<Function*> findFunctionsToInstrument(Module &M, Noelle &noelle);
void printFunctions(std::unordered_set<Function*> &functions);
