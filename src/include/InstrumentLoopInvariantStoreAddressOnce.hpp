#pragma once

#ifndef INSTRUMENT_LOOP_INVARIANT_STORE_ADDRESS_ONCE
#define INSTRUMENT_LOOP_INVARIANT_STORE_ADDRESS_ONCE

#include "StoreLoopInstrumentation.hpp"

void getLoopInvariantLoadStoreAddresses(Module &M, Noelle &noelle, std::unordered_set<Instruction*> &instsToSkip, std::unordered_map<Instruction*, Entry*> &instToEntryMap, std::unordered_set<Instruction*> &instrumentedInsts, std::unordered_map<LoadStoreInstInfo*, std::unordered_set<ExitBasicBlockInfo*>> &instsToInstrument);

#endif
