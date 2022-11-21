#include "./include/ClusterCallstackInstrumentation.hpp"

void visitDepthFirst(CallGraphFunctionNode *node, std::unordered_set<Function*> &uniqueCallstackFunctions){
  if (node->getIncomingEdges().size() > 1){
    return;
  }

  uniqueCallstackFunctions.insert(node->getFunction());
  
  for (auto outgoingEdge : node->getOutgoingEdges()){
    CallGraphFunctionNode *nextNode = outgoingEdge->getCallee();
    visitDepthFirst(nextNode, uniqueCallstackFunctions);
  }

  return;
}

std::unordered_set<Function*> getUniqueCallstackFunctions(Noelle &noelle){
  std::unordered_set<Function*> uniqueCallstackFunctions;

  // Fetch callgraph
  auto functionsManager = noelle.getFunctionsManager();
  auto callGraph = functionsManager->getProgramCallGraph();

  auto entryNode = callGraph->getEntryNode();
  visitDepthFirst(entryNode, uniqueCallstackFunctions);

  return uniqueCallstackFunctions;
}

std::map<Function*, Instruction*> callstackInstrumentation(Module &M, std::unordered_set<Function*> &uniqueCallstackFunctions){
  std::map<Function*, Instruction*> funcToCallstack;

  // Get the runtime function
  Function *runtimeFuncCallstack = getRuntimeFunction(M, MEMORYTOOL_CALLSTACK);
  Function *runtimeFuncCallstackUnique = getRuntimeFunction(M, MEMORYTOOL_CALLSTACK_UNIQUE);

  // Get some default values we need later on
  LLVMContext &context = M.getContext();
  Type *int64Type = Type::getInt64Ty(context);
  const Twine &NameStr = "";

  uint64_t index = 0;
  uint64_t uniqueCallstackFunctionsSize = uniqueCallstackFunctions.size();
  // Iterate over functions in current module
  for (Function &F : M){
    // Check if we have the body of the function
    if (F.empty()){
      continue;
    }

    // Check that is not an intrinsic function
    if (F.isIntrinsic()){
      continue;
    }

    // Get entry basic block
    BasicBlock &entryBB = F.getEntryBlock();

    // Get entry instruction
    Instruction &entryI = entryBB.front();

    // Insert call to build callstack as the very first instruction the current function executes
    CallInst *callinstCallstack = nullptr;
    if (uniqueCallstackFunctions.count(&F)){
      Constant *indexConstant = ConstantInt::get(int64Type, index);
      Constant *uniqueCallstackFunctionsSizeConstant = ConstantInt::get(int64Type, uniqueCallstackFunctionsSize);

      std::vector<Value *> callVals;
      callVals.push_back(indexConstant);
      callVals.push_back(uniqueCallstackFunctionsSizeConstant);

      ArrayRef<Value *> callArgs = ArrayRef<Value *>(callVals);
      callinstCallstack = CallInst::Create(runtimeFuncCallstackUnique, callArgs, NameStr, &entryI);

      index += 1;
    } else {
      callinstCallstack = CallInst::Create(runtimeFuncCallstack, NameStr, &entryI);
    }

    // Insert call inst into the map
    funcToCallstack[&F] = callinstCallstack;
  }

  return funcToCallstack;
}

