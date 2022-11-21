#include "./include/CloneFunction.hpp"

#include <vector>
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "srcCARMOT/include/Utils.hpp"

Function* cloneFunction(Function *func){
  errs() << "CLONING " << func->getName() << "\n"; 
  ValueToValueMapTy vMap;
  Function *clonedFunc = CloneFunction(func, vMap);
  StringRef funcNameMangled = func->getName();
  clonedFunc->setName(funcNameMangled + "_cloned");
  return clonedFunc;
}

bool cloneFunctions(Module &M){
  bool modified = false;

  std::vector<Function*> functionsList;
  for (auto &F : M){
    functionsList.push_back(&F);
  }

  for (auto F : functionsList){
    //if (F->getName() == MAIN){
    //  continue;
    //}

    if (F->isIntrinsic()){
      continue;
    }

    if (F->empty()){
      continue;
    }

    Function *funcCloned = cloneFunction(F);
    //M.getFunctionList().push_back(funcCloned);
    
    modified = true;
  }

  return modified;
}
