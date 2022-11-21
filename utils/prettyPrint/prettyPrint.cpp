#include "help.hpp"

std::string printDebugData(DebugData *debugData, Trace *trace){
  std::stringstream ss;

  ss << "\tpathToFile = " << debugData->pathToFile << std::endl;
  ss << "\tvarName = " << debugData->varName << std::endl;
  ss << "\tlineNum = " << debugData->lineNum << std::endl;
  ss << "\tcolumnNum = " << debugData->columnNum << std::endl;

  if (trace == nullptr){
    return ss.str();
  }

  void *buffer[TRACE_SIZE];
  copyCallstack(buffer, trace);
  std::string back_trace = print_backtrace(buffer, trace->nptrs, execPathGlobal);
  ss << "\tcallstack =\n" << back_trace << std::endl;

  return ss.str();
}

std::string printUseToSave(UseToSave &useToSave, std::unordered_map<uint64_t, DebugData*> &debugData){
  std::stringstream ss;
  Trace *trace = useToSave.trace;
  uint64_t staticID = useToSave.staticID;
  if (debugData.count(staticID) == 0){
    std::cerr << "ERROR: staticID " << staticID << " not found in debugData. Abort.\n";
    abort();
  }
  DebugData *debugDatum = debugData[staticID];
  std::string res = printDebugData(debugDatum, trace);
  ss << "\tuniqueID = " << useToSave.uniqueID << "\n";
  ss << res;

  return ss.str();
}

std::string printUsesToSave(std::vector<UseToSave> &usesToSave, std::unordered_map<uint64_t, DebugData*> &debugData){
  std::stringstream ss;
  ss << "USES\n";
  for (auto &useToSave : usesToSave){
    ss << printUseToSave(useToSave, debugData);
  }

  return ss.str();
}

std::string printOffsets(std::unordered_set<uint64_t> &offsets){
  std::stringstream ss;
  ss << "OFFSETS of this allocation: ";
  for (auto offset : offsets){
    ss << offset << ",";
  }
  ss << "\n";

  return ss.str();
}

void printAllocationToSave(AllocationToSave &allocationToSave,  std::unordered_map<uint64_t, DebugData*> &debugData){
  std::stringstream ss;

  Trace *trace = allocationToSave.trace;
  uint64_t staticID = allocationToSave.staticID;
  if (debugData.count(staticID) == 0){
    std::cerr << "ERROR: staticID " << staticID << " not found in debugData. Abort.\n";
    abort();
  }
  DebugData *debugDatum = debugData[staticID];
  ss << "ALLOCATION\n";
  ss << "\tuniqueID = " << allocationToSave.uniqueID; 
  ss << "\n";
  ss << "\tpointer = " << (void*) allocationToSave.pointer << " length = " << allocationToSave.length; 
  ss << "\n";
  ss << "\t" << printOffsets(allocationToSave.offsets);
  ss << printDebugData(debugDatum, trace);
  ss << "\n";
  ss << printUsesToSave(allocationToSave.uses, debugData);
  ss << "\n";

  std::cout << ss.str();

  return;
}

/*
uint64_t hashAllocation(AllocationToSave &allocationToSave){
  uint64_t hash = ((uint64_t) allocationToSave.trace) ^ (allocationToSave.staticID) ^ ((uint64_t) allocationToSave.offsets.size()) ^ ((uint64_t) allocationToSave.uses.size());

  return hash;
}

bool compareUses(std::vector<UseToSave> &uses1, std::vector<UseToSave> &uses2){
  if (uses1.size() != uses2.size()){
    return false;
  }

  for (auto &use1 : uses1){
    bool found = false;
    for (auto &use2 : uses2){
      if (use1.staticID != use2.staticID){
        continue;
      }

      if ((use1.trace != nullptr) && (use2.trace != nullptr)){
        if ((traceCompare(*(use1.trace), *(use2.trace))) && (use1.staticID == use2.staticID)){
          found = true;
          break;
        }
      }
    }
    if (!found){
      return false;
    }
  }

  return true;
}

std::vector<AllocationToSave*> removeDuplicates(StateSetToSave &stateSetToSave){
  std::vector<AllocationToSave*> noDuplicates;
  std::unordered_map<uint64_t, std::vector<AllocationToSave*>> collisionMap;

  for (auto &alloc : stateSetToSave.allocations){
    uint64_t hash = hashAllocation(alloc);
    auto hashInMap = collisionMap.find(hash);
    if (hashInMap == collisionMap.end()){ // not found
      collisionMap[hash].push_back(&alloc);
    } else {
      bool found = false;
      for (auto elem : hashInMap->second){
        if (alloc.staticID != elem->staticID){
          continue;
        }

        if ((alloc.trace != nullptr) && (elem->trace != nullptr)){
          if (!traceCompare(*(alloc.trace), *(elem->trace))){
            continue;
          }
        }

        if (alloc.offsets != elem->offsets){
          continue;
        }

        if (!compareUses(alloc.uses, elem->uses)){
          continue;
        }

        found = true;
        break;
      }
      if (!found){
        collisionMap[hash].push_back(&alloc);
      }
    }
  }

  for (auto entry : collisionMap){
    for (auto elem : entry.second){
      noDuplicates.push_back(elem);
    }
  }

  return noDuplicates;
}
*/

void printStateSetToSave(StateSetToSave &stateSetToSave,  std::unordered_map<uint64_t, DebugData*> &debugData, std::string setName){
  std::cout << setName << "\n";
  for (auto &alloc : stateSetToSave.allocations){
    printAllocationToSave(alloc, debugData);
  }
  std::cout << "\n";

  return;
}

/*
void printStateSetToSave(StateSetToSave &stateSetToSave,  std::unordered_map<uint64_t, DebugData*> &debugData, std::string setName){
  std::cout << setName << "\n";

  std::vector<std::string> set;
  //auto noDuplicateAllocations = removeDuplicates(stateSetToSave);

  //for (auto alloc : noDuplicateAllocations){
  for (auto alloc : stateSetToSave.allocations){
    set.push_back(printAllocationToSave(alloc, debugData));
  }

  // Print
  for (auto &elem : set){
    std::cout << elem;
  }

  std::cout << "\n";

  return;
}
*/

void printStateToSave(StateToSave &stateToSave,  std::unordered_map<uint64_t, DebugData*> &debugData){
  printStateSetToSave(stateToSave.I, debugData, "Set I");
  printStateSetToSave(stateToSave.O, debugData, "Set O");
  printStateSetToSave(stateToSave.IO, debugData, "Set IO");
  printStateSetToSave(stateToSave.CO, debugData, "Set CO");
  printStateSetToSave(stateToSave.TO, debugData, "Set TO");
  printStateSetToSave(stateToSave.CIO, debugData, "Set CIO");
  printStateSetToSave(stateToSave.TIO, debugData, "Set TIO");

  return;
}

void printStatesToSave(StatesToSave &statesToSave, std::unordered_map<uint64_t, DebugData*> &debugData){
  for (auto &elem : statesToSave.states){
    StateToSave &stateToSave = elem.second;
    std::cout << "ROI at function " << stateToSave.functionName << " line " << stateToSave.lineNumber << "\n";
    printStateToSave(stateToSave, debugData);
    std::cout << "\n";
  }

  return;
}

int main (int argc, char* argv[]){
  std::string exec_path(argv[1]);
  execPathGlobal = exec_path;

  // Load files data
  std::unordered_map<uint64_t, DebugData*> *debugData = getDebugData();
  StatesToSave *statesToSave = getRunData();

  printStatesToSave(*statesToSave, *debugData);

  return 0;
}
