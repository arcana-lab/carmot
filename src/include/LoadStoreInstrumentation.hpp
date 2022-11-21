#pragma once

#ifndef LOAD_STORE_INSTRUMENTATION
#define LOAD_STORE_INSTRUMENTATION

#include "Utils.hpp"

class LoadStoreInstrumentation{
  public:
    CastInst *pointerCast;
    CallInst *addToState;

    LoadStoreInstrumentation(CastInst *pointerCast, CallInst *addToState):pointerCast{pointerCast},addToState{addToState}{}

};

LoadStoreInstrumentation* getLoadStoreInstrumentation(Module &M, Entry *entry, std::map<Function*, Instruction*> &funcToCallstack);

#endif
