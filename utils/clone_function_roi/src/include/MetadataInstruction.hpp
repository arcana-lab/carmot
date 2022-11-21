#pragma once

#ifndef METADATA_INSTRUCTION
#define METADATA_INSTRUCTION

#include <string>
#include "llvm/IR/Instructions.h"

using namespace llvm;

bool hasMetadata(Instruction *inst, std::string metadataName);
bool setMetadata(Instruction *inst, std::string metadataName, std::string metadataValue);
std::string getMetadata(Instruction *inst, std::string metadataName);
void deleteMetadata(Instruction *inst, std::string metadataName);


#endif
