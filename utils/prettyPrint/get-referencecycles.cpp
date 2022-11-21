#include "help.hpp"

void printDebugData(DebugData *debugData, Trace *trace){
  std::cout << "\tpathToFile = " << debugData->pathToFile << std::endl;
  std::cout << "\tvarName = " << debugData->varName << std::endl;
  std::cout << "\tlineNum = " << debugData->lineNum << std::endl;
  std::cout << "\tcolumnNum = " << debugData->columnNum << std::endl;

  if (trace == nullptr){
    return;
  }

  void *buffer[TRACE_SIZE];
  copyCallstack(buffer, trace);
  std::string back_trace = print_backtrace(buffer, trace->nptrs, execPathGlobal);
  std::cout << "\tcallstack =\n" << back_trace << std::endl;

  return;
}

void printAllocationToSave(AllocationToSave &allocationToSave,  std::unordered_map<uint64_t, DebugData*> &debugData){
  Trace *trace = allocationToSave.trace;
  uint64_t staticID = allocationToSave.staticID;
  if (debugData.count(staticID) == 0){
    std::cerr << "ERROR: staticID " << staticID << " not found in debugData. Abort.\n";
    abort();
  }
  DebugData *debugDatum = debugData[staticID];
  std::cout << "ALLOCATION\n";
  printDebugData(debugDatum, trace);
  std::cout << "\n";

  return;
}

void printCycle(CycleToSave &cycleToSave, std::unordered_map<uint64_t, DebugData*> &debugData){
  std::cout << "Allocations creating a cycle:\n";
  for (auto &allocationToSave : cycleToSave.cycle){
    printAllocationToSave(allocationToSave, debugData);
  }

  std::cout << "The allocation to transform into a weak pointer is:\n";
  printAllocationToSave(cycleToSave.weakPoint, debugData);
  std::cout << "\n";

  return;
}

void printCycles(StatesToSave &statesToSave, std::unordered_map<uint64_t, DebugData*> &debugData){
  for (auto &elem : statesToSave.states){
 
    StateToSave &stateToSave = elem.second;
    std::cout << "ROI at function " << stateToSave.functionName << " line " << stateToSave.lineNumber << "\n";
   
    for (auto &cycleToSave : stateToSave.cycles){
      printCycle(cycleToSave, debugData);
    }
  }

  return;
}

int main (int argc, char* argv[]){
  std::string exec_path(argv[1]);
  execPathGlobal = exec_path;

  std::unordered_map<uint64_t, DebugData*> *debugData = getDebugData();
  StatesToSave *statesToSave = getRunData();
  printCycles(*statesToSave, *debugData);

  // Clean memory
  delete debugData;
  delete statesToSave;

  return 0;
}
