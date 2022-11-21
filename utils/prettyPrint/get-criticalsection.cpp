#include "help.hpp"

std::string printDebugData(DebugData *debugData, Trace *trace){
  std::string pathToFile = debugData->pathToFile;
  unsigned lineNum = debugData->lineNum;
  unsigned columnNum = debugData->columnNum;
  std::fstream infile;
  infile.open(pathToFile, std::ios::in);
  if (!infile.is_open()){
    std::cerr << "ERROR: file " << pathToFile << " not found. Abort.\n";
    abort();
  }

  std::string line;
  auto currLineNum = 1;
  while (std::getline(infile, line)){
    if (currLineNum == lineNum){
      break;
    }
    currLineNum += 1;
  }
  infile.close();

  return line;
}

std::string printUseToSave(UseToSave &useToSave, std::unordered_map<uint64_t, DebugData*> &debugData, std::unordered_map<std::string, DebugData*> &lineInfoMap){
  Trace *trace = useToSave.trace;
  uint64_t staticID = useToSave.staticID;
  if (debugData.count(staticID) == 0){
    std::cerr << "ERROR: staticID " << staticID << " not found in debugData. Abort.\n";
    abort();
  }
  DebugData *debugDatum = debugData[staticID];
  std::string line = printDebugData(debugDatum, trace);
  if (lineInfoMap.count(line) == 0){
    lineInfoMap[line] = debugDatum;
  }
  if (debugDatum->lineNum != 0){
    lineInfoMap[line] = debugDatum;
  }

  return line;
}

void printUsesToSave(std::vector<UseToSave> &usesToSave, std::unordered_map<uint64_t, DebugData*> &debugData, std::unordered_set<std::string> &linesSet, std::unordered_map<std::string, DebugData*> &lineInfoMap){
  for (auto &useToSave : usesToSave){
    linesSet.insert(printUseToSave(useToSave, debugData, lineInfoMap));
  }

  return;
}

void printAllocationToSave(AllocationToSave &allocationToSave,  std::unordered_map<uint64_t, DebugData*> &debugData, std::unordered_set<std::string> &linesSet, std::unordered_map<std::string, DebugData*> &lineInfoMap){
  Trace *trace = allocationToSave.trace;
  uint64_t staticID = allocationToSave.staticID;
  if (debugData.count(staticID) == 0){
    std::cerr << "ERROR: staticID " << staticID << " not found in debugData. Abort.\n";
    abort();
  }
  printUsesToSave(allocationToSave.uses, debugData, linesSet, lineInfoMap);

  return;
}

void printStateSetToSave(StateSetToSave &stateSetToSave,  std::unordered_map<uint64_t, DebugData*> &debugData, std::string setName, std::unordered_set<std::string> &linesSet, std::unordered_map<std::string, DebugData*> &lineInfoMap){
  if (!setName.empty()){
    std::cout << setName << "\n";
  }
  for (auto &alloc : stateSetToSave.allocations){
    printAllocationToSave(alloc, debugData, linesSet, lineInfoMap);
  }

  return;
}

void printStateToSave(StateToSave &stateToSave,  std::unordered_map<uint64_t, DebugData*> &debugData){
  std::unordered_set<std::string> linesSet;
  std::unordered_map<std::string, DebugData*> lineInfoMap;
  printStateSetToSave(stateToSave.TO, debugData, "", linesSet, lineInfoMap);
  printStateSetToSave(stateToSave.TIO, debugData, "", linesSet, lineInfoMap);

  std::string pragma("#pragma omp critical");
  for (auto &line : linesSet){
    std::cout << pragma << "\n";
    std::cout << lineInfoMap[line]->pathToFile << ":" << lineInfoMap[line]->lineNum << "\t" << line << "\n";
  }

  return;
}

void printStatesToSave(StatesToSave &statesToSave, std::unordered_map<uint64_t, DebugData*> &debugData){
  auto i = 0;
  for (auto &stateToSave : statesToSave.states){
    std::cout << "ROI number " << i << "\n";
    printStateToSave(stateToSave.second, debugData);
    i += 1;
    std::cout << "\n";
  }

  return;
}

int main (int argc, char* argv[]){
  std::string exec_path(argv[1]);
  execPathGlobal = exec_path;

  std::unordered_map<uint64_t, DebugData*> *debugData = getDebugData();
  StatesToSave *statesToSave = getRunData();
  printStatesToSave(*statesToSave, *debugData);

  // Clean memory
  delete debugData;
  delete statesToSave;

  return 0;
}
