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

void printStateSetToSave(StateSetToSave &stateSetToSave,  std::unordered_map<uint64_t, DebugData*> &debugData, std::string setName){
  std::cout << setName << "\n";
  for (auto &alloc : stateSetToSave.allocations){
    printAllocationToSave(alloc, debugData);
  }
  std::cout << "\n";

  return;
}

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

void removeDuplicatesFromSet(StateSetToSave &stateSetToSave){
  std::unordered_set<uint64_t> uniqueIDs;
  std::vector<AllocationToSave> &allocations = stateSetToSave.allocations;

  auto it = allocations.begin();
  uint64_t i = 0;
  while(it != allocations.end()){
    AllocationToSave &allocation = *it;
    uint64_t uniqueID = allocation.uniqueID;
    if (uniqueIDs.count(uniqueID) == 0){
      uniqueIDs.insert(uniqueID);
      ++it;
      continue;
    }

    it = allocations.erase(it);
  }

  return;
}

void removeDuplicates(StatesToSave *statesToSave){
  std::cerr << "STATES = " << statesToSave->states.size() << "\n";
  for (auto &elem : statesToSave->states){
    StateToSave &stateToSave = elem.second;
    std::cerr << "Removing duplicates for I\n";
    removeDuplicatesFromSet(stateToSave.I);
    std::cerr << "Removing duplicates for O\n";
    removeDuplicatesFromSet(stateToSave.O);
    std::cerr << "Removing duplicates for IO\n";
    removeDuplicatesFromSet(stateToSave.IO);
    std::cerr << "Removing duplicates for CO\n";
    removeDuplicatesFromSet(stateToSave.CO);
    std::cerr << "Removing duplicates for TO\n";
    removeDuplicatesFromSet(stateToSave.TO);
    std::cerr << "Removing duplicates for CIO\n";
    removeDuplicatesFromSet(stateToSave.CIO);
    std::cerr << "Removing duplicates for TIO\n";
    removeDuplicatesFromSet(stateToSave.TIO);
  }

  return;
}

void fixSet(StateSetToSave &src1, StateSetToSave &src2, StateSetToSave &dst){
  std::vector<AllocationToSave*> allocationsToAdd;
  std::vector<AllocationToSave> &allocations1 = src1.allocations;
  std::vector<AllocationToSave> &allocations2 = src2.allocations;
  auto it1 = allocations1.begin();
  while(it1 != allocations1.end()){
    bool foundDuplicate = false;
    AllocationToSave &allocation1 = *it1;
    uint64_t uniqueID1 = allocation1.uniqueID;
    auto it2 = allocations2.begin();
    while(it2 != allocations2.end()){
      AllocationToSave &allocation2 = *it2;
      uint64_t uniqueID2 = allocation2.uniqueID;
      if (uniqueID1 == uniqueID2){
        // Insert allocation in dst
        AllocationToSave *newAllocation = new AllocationToSave(allocation1);
        allocationsToAdd.push_back(newAllocation);

        foundDuplicate = true;
      }
      if (foundDuplicate){
        // Remove allocations from sets
        it2 = allocations2.erase(it2);
        break; // we have no duplicates in a single set
      } else {
        ++it2;
      }
    }

    if (foundDuplicate){
      it1 = allocations1.erase(it1);
    } else {
      ++it1;
    }

  }

  std::vector<AllocationToSave> &allocationsDst = dst.allocations;
  for (auto elem : allocationsToAdd){
    allocationsDst.push_back(*elem);
  }

  return;
}

void fixSets(StateToSave &stateToSave){
  std::cerr << "Fixing set I\n";
  fixSet(stateToSave.I, stateToSave.O, stateToSave.IO);
  fixSet(stateToSave.I, stateToSave.CO, stateToSave.CIO);
  fixSet(stateToSave.I, stateToSave.TO, stateToSave.TIO);
  fixSet(stateToSave.I, stateToSave.IO, stateToSave.IO);
  fixSet(stateToSave.I, stateToSave.CIO, stateToSave.CIO);
  fixSet(stateToSave.I, stateToSave.TIO, stateToSave.TIO);

  std::cerr << "Fixing set O\n";
  fixSet(stateToSave.O, stateToSave.CO, stateToSave.CO);
  fixSet(stateToSave.O, stateToSave.TO, stateToSave.TO);
  fixSet(stateToSave.O, stateToSave.IO, stateToSave.IO);
  fixSet(stateToSave.O, stateToSave.CIO, stateToSave.CIO);
  fixSet(stateToSave.O, stateToSave.TIO, stateToSave.TIO);

  std::cerr << "Fixing set IO\n";
  fixSet(stateToSave.IO, stateToSave.CO, stateToSave.CIO);
  fixSet(stateToSave.IO, stateToSave.TO, stateToSave.TIO);
  fixSet(stateToSave.IO, stateToSave.CIO, stateToSave.CIO);
  fixSet(stateToSave.IO, stateToSave.TIO, stateToSave.TIO);

  std::cerr << "Fixing set CO\n";
  fixSet(stateToSave.CO, stateToSave.TO, stateToSave.TO);
  fixSet(stateToSave.CO, stateToSave.CIO, stateToSave.CIO);
  fixSet(stateToSave.CO, stateToSave.TIO, stateToSave.TIO);

  std::cerr << "Fixing set TO\n";
  fixSet(stateToSave.TO, stateToSave.CIO, stateToSave.TIO);
  fixSet(stateToSave.TO, stateToSave.TIO, stateToSave.TIO);

  std::cerr << "Fixing set CIO\n";
  fixSet(stateToSave.CIO, stateToSave.TIO, stateToSave.TIO);

  return;
}

void fixSetsAll(StatesToSave *statesToSave){
  for (auto &elem : statesToSave->states){
    StateToSave &stateToSave = elem.second;
    fixSets(stateToSave);
  }

  return;
}

std::unordered_set<std::string> getAllocations(StateSetToSave &stateSetToSave, std::unordered_map<uint64_t, DebugData*> &debugData){
  std::unordered_set<std::string> allocations;
  for (auto &alloc : stateSetToSave.allocations){
    uint64_t staticID = alloc.staticID;
    DebugData *debugDatum = debugData[staticID];
    std::string varName(debugDatum->varName);
    if (varName.empty()){
      varName = std::to_string(debugDatum->lineNum);
    }
    allocations.insert(varName);
  }

  return allocations;
}

void printPragma(StateToSave &stateToSave,  std::unordered_map<uint64_t, DebugData*> &debugData){
  std::unordered_set<std::string> sharedAttr = getAllocations(stateToSave.I, debugData);

  std::unordered_set<std::string> privateAttr;
  std::unordered_set<std::string> privateAttrTmp1 = getAllocations(stateToSave.O, debugData);
  std::set_union(privateAttrTmp1.begin(), privateAttrTmp1.end(), privateAttr.begin(), privateAttr.end(), std::inserter(privateAttr, privateAttr.begin()));
  std::unordered_set<std::string> privateAttrTmp2 = getAllocations(stateToSave.IO, debugData);
  std::set_union(privateAttrTmp2.begin(), privateAttrTmp2.end(), privateAttr.begin(), privateAttr.end(), std::inserter(privateAttr, privateAttr.begin()));
  std::unordered_set<std::string> privateAttrTmp3 = getAllocations(stateToSave.CO, debugData);
  std::set_union(privateAttrTmp3.begin(), privateAttrTmp3.end(), privateAttr.begin(), privateAttr.end(), std::inserter(privateAttr, privateAttr.begin()));
  std::unordered_set<std::string> privateAttrTmp4 = getAllocations(stateToSave.CIO, debugData);
  std::set_union(privateAttrTmp4.begin(), privateAttrTmp4.end(), privateAttr.begin(), privateAttr.end(), std::inserter(privateAttr, privateAttr.begin()));

  std::unordered_set<std::string> transferAttr;
  std::unordered_set<std::string> transferAttrTmp1 = getAllocations(stateToSave.TO, debugData);
  std::set_union(transferAttrTmp1.begin(), transferAttrTmp1.end(), transferAttr.begin(), transferAttr.end(), std::inserter(transferAttr, transferAttr.begin()));
  std::unordered_set<std::string> transferAttrTmp2 = getAllocations(stateToSave.TIO, debugData);
  std::set_union(transferAttrTmp2.begin(), transferAttrTmp2.end(), transferAttr.begin(), transferAttr.end(), std::inserter(transferAttr, transferAttr.begin()));

  std::cout << "#pragma omp parallel for\n";
  std::cout << "shared:\n";
  std::cout << "I: ";
  for (auto &elem : sharedAttr){
    std::cout << elem << ", ";
  }
  std::cout << "\n";

  std::cout << "private:\n";
  std::cout << "O: ";
  for (auto &elem : privateAttrTmp1){
    std::cout << elem << ", ";
  }
  std::cout << "\n";
  std::cout << "IO: ";
  for (auto &elem : privateAttrTmp2){
    std::cout << elem << ", ";
  }
  std::cout << "\n";
  std::cout << "CO: ";
  for (auto &elem : privateAttrTmp3){
    std::cout << elem << ", ";
  }
  std::cout << "\n";
  std::cout << "CIO: ";
  for (auto &elem : privateAttrTmp4){
    std::cout << elem << ", ";
  }
  std::cout << "\n";
 

  std::cout << "transfer:\n";
  std::cout << "TO: ";
  for (auto &elem : transferAttrTmp1){
    std::cout << elem << ", ";
  }
  std::cout << "\n";
  std::cout << "TIO: ";
  for (auto &elem : transferAttrTmp2){
    std::cout << elem << ", ";
  }
  std::cout << "\n";

  return;
}

void printPragmas(StatesToSave &statesToSave, std::unordered_map<uint64_t, DebugData*> &debugData){
  for (auto &elem : statesToSave.states){
    StateToSave &stateToSave = elem.second;
    std::cout << "ROI at function " << stateToSave.functionName << " line " << stateToSave.lineNumber << "\n";
    printPragma(stateToSave, debugData);
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

  // Remove duplicates from sets
  removeDuplicates(statesToSave);
  // Fix sets following "lattice"
  fixSetsAll(statesToSave);
  // Remove newly create duplicates
  removeDuplicates(statesToSave);

  // Print no duplicate and fixed sets
  printStatesToSave(*statesToSave, *debugData);

  // Print pragmas
  printPragmas(*statesToSave, *debugData);

  // Clean memory
  delete debugData;
  delete statesToSave;

  return 0;
}
