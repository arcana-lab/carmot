#include "./include/Utils.hpp"
#include "./include/AvoidLocalVariableInstrumentation.hpp"

std::unordered_set<Function*> findFunctionsToInstrument(Module &M){
  std::unordered_set<Function*> functions;
  for (auto &F : M){
    bool toOptimize = F.hasMetadata(OPTIMIZE_LOCALS_KIND);
    if (toOptimize){
      functions.insert(&F);

      // Remove opt-none attribute, it's not needed anymore
      F.removeFnAttr(Attribute::OptimizeNone);
    }
  }

  return functions;
}
