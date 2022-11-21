#include "include/MetadataInstruction.hpp"

#include "llvm/IR/Metadata.h"
#include "llvm/IR/LLVMContext.h"

bool hasMetadata(Instruction *inst, std::string metadataName){
  MDNode *n = inst->getMetadata(metadataName);
  if (n == nullptr){
    return false;
  }

  return true;
}

bool setMetadata(Instruction *inst, std::string metadataName, std::string metadataValue){
  auto& cxt = inst->getContext();
  MDString *s = MDString::get(cxt, metadataValue);
  MDNode *n = inst->getMetadata(metadataName);
  if (n != nullptr){
    n->replaceOperandWith(0, s);
  } else {
    n = MDNode::get(cxt, s);
    inst->setMetadata(metadataName, n);
  }

  // We changed the IR, so return true
  return true;
}

std::string getMetadata(Instruction *inst, std::string metadataName){
  MDNode *n = inst->getMetadata(metadataName);
  if (n == nullptr){
    errs() << "ERROR: metadata " << metadataName << " does not exist. Abort.\n";
    abort();
  }

  MDString *s = cast<MDString>(n->getOperand(0));
  std::string metadataValue = s->getString().str();

  return metadataValue;
}

void deleteMetadata(Instruction *inst, std::string metadataName){
  MDNode *n = inst->getMetadata(metadataName);
  if (n == nullptr){
    return;
  }

  inst->setMetadata(metadataName, nullptr);

  return;
}
