#pragma once

#ifndef BASE_ADDRESS_MOSTLY_INVARIANT
#define BASE_ADDRESS_MOSTLY_INVARIANT

#include "Utils.hpp"
#include <unordered_set>

std::unordered_set<Instruction*> getBaseAddressMostlyInvariants(std::unordered_set<Instruction *> &instsToSkip, std::unordered_map<Instruction*, Entry*> &instToEntryMap);

#endif
