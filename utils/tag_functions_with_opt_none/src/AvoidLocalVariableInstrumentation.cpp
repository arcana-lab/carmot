#include "../../../src/include/Utils.hpp"
#include "./include/AvoidLocalVariableInstrumentation.hpp"

void visit(CallGraphFunctionNode *currentNode, std::unordered_set<Function*> &functions){

  // Check if currentNode was already visited
  auto currentNodeFunction = currentNode->getFunction();
  if (functions.count(currentNodeFunction)){
    return;
  }
  functions.insert(currentNodeFunction);

  // Visit other nodes with incoming edges
  for(auto incomingEdge : currentNode->getIncomingEdges()){
    auto nextNode = incomingEdge->getCaller();
    visit(nextNode, functions);
  }

  return;
}

std::unordered_set<Function*> getAllFunctions(Module &M){
  std::unordered_set<Function*> functionsToReturn;
  for (auto &F : M){
    if (F.empty()){
      continue;
    }

    functionsToReturn.insert(&F);
  }

  return functionsToReturn;
}

std::unordered_set<Function*> findFunctionsToInstrument(Module &M, Noelle &noelle){
  std::unordered_set<Function*> functions;
  std::unordered_set<Function*> functionsToReturn;

  // Fetch callgraph
  errs() << "ED before functionsManager\n";
  auto functionsManager = noelle.getFunctionsManager();
  errs() << "ED before programCallGraph\n";
  auto callGraph = functionsManager->getProgramCallGraph();

  // Fetch starting node
  auto startTrackingFunction = M.getFunction(MEMORYTOOL_START_TRACKING);
  if (!startTrackingFunction){
    errs() << "WARNING: no start tracking function found.\n";
    return functions;
  }
  errs() << "ED before getFunctionNode\n";
  auto startTrackingFunctionNode = callGraph->getFunctionNode(startTrackingFunction);

  // Visit all paths from start tracking to program's entry
  errs() << "ED before visit\n";
  visit(startTrackingFunctionNode, functions);

  // Remove all empty functions
  for (auto function : functions){
    if (function->empty()){
      continue;
    }

    functionsToReturn.insert(function); 
  }

  return functionsToReturn;
}

void printFunctions(std::unordered_set<Function*> &functions){
  errs() << "Functions in the set:\n";
  for (auto elem : functions){
    errs() << elem->getName() << " ";
  }
  errs() << "\n";

  return;
}
