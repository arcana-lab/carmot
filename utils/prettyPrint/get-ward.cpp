#include "help.hpp"

bool isThereIntersection(std::vector<AllocationToSave> &set1, std::vector<AllocationToSave> &set2){
  std::set<uint64_t> set1UniqueIDs;
  std::set<uint64_t> set2UniqueIDs;

  for (auto &elem : set1){
    set1UniqueIDs.insert(elem.uniqueID);
  }

  for (auto &elem : set2){
    set2UniqueIDs.insert(elem.uniqueID);
  }

  std::set<uint64_t> intersection;
  std::set_intersection(set1UniqueIDs.begin(), set1UniqueIDs.end(), set2UniqueIDs.begin(), set2UniqueIDs.end(), std::inserter(intersection, intersection.begin()));

  bool thereIsIntersection = (intersection.size() > 0);

  return thereIsIntersection;
}

void sanityCheck(StateToSave &stateToSave){
  bool intersection = false;

  intersection |= isThereIntersection(stateToSave.I.allocations, stateToSave.O.allocations);
  intersection |= isThereIntersection(stateToSave.I.allocations, stateToSave.IO.allocations);
  intersection |= isThereIntersection(stateToSave.I.allocations, stateToSave.CO.allocations);
  intersection |= isThereIntersection(stateToSave.I.allocations, stateToSave.TO.allocations);
  intersection |= isThereIntersection(stateToSave.I.allocations, stateToSave.CIO.allocations);
  intersection |= isThereIntersection(stateToSave.I.allocations, stateToSave.TIO.allocations);

  intersection |= isThereIntersection(stateToSave.O.allocations, stateToSave.IO.allocations);
  intersection |= isThereIntersection(stateToSave.O.allocations, stateToSave.CO.allocations);
  intersection |= isThereIntersection(stateToSave.O.allocations, stateToSave.TO.allocations);
  intersection |= isThereIntersection(stateToSave.O.allocations, stateToSave.CIO.allocations);
  intersection |= isThereIntersection(stateToSave.O.allocations, stateToSave.TIO.allocations);

  intersection |= isThereIntersection(stateToSave.IO.allocations, stateToSave.CO.allocations);
  intersection |= isThereIntersection(stateToSave.IO.allocations, stateToSave.TO.allocations);
  intersection |= isThereIntersection(stateToSave.IO.allocations, stateToSave.CIO.allocations);
  intersection |= isThereIntersection(stateToSave.IO.allocations, stateToSave.TIO.allocations);

  intersection |= isThereIntersection(stateToSave.CO.allocations, stateToSave.TO.allocations);
  intersection |= isThereIntersection(stateToSave.CO.allocations, stateToSave.CIO.allocations);
  intersection |= isThereIntersection(stateToSave.CO.allocations, stateToSave.TIO.allocations);

  intersection |= isThereIntersection(stateToSave.TO.allocations, stateToSave.CIO.allocations);
  intersection |= isThereIntersection(stateToSave.TO.allocations, stateToSave.TIO.allocations);

  intersection |= isThereIntersection(stateToSave.CIO.allocations, stateToSave.TIO.allocations);

  if (intersection){
    std::cerr << "Found an intersection, sanity check failed. Abort.\n";
    abort();
  }

  return;
}

double getPercentageOfWardAllocations(StateToSave &stateToSave){
  uint64_t totalAllocationsNotInTransferSet = stateToSave.I.allocations.size() + stateToSave.O.allocations.size() + stateToSave.IO.allocations.size() + stateToSave.CO.allocations.size() + stateToSave.CIO.allocations.size();
  uint64_t totalAllocations = totalAllocationsNotInTransferSet + stateToSave.TO.allocations.size() + stateToSave.TIO.allocations.size();

  return (((double) totalAllocationsNotInTransferSet) / ((double) totalAllocations));
}

void processStateToSave(StateToSave &stateToSave){
  sanityCheck(stateToSave);
  double percentageOfWardAllocations = getPercentageOfWardAllocations(stateToSave);
  std::cout << "percentage of allocations that are WARD " << percentageOfWardAllocations << "\n";

  return;
}

void processStatesToSave(StatesToSave &statesToSave){
  for (auto &elem : statesToSave.states){
    StateToSave &stateToSave = elem.second;
    std::cout << "ROI at function " << stateToSave.functionName << " line " << stateToSave.lineNumber << "\n";
    processStateToSave(stateToSave);
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

int main (int argc, char* argv[]){
  std::string exec_path(argv[1]);
  execPathGlobal = exec_path;

  // Load files data
  StatesToSave *statesToSave = getRunData();

  // Remove duplicates from sets
  removeDuplicates(statesToSave);
  // Fix sets following "lattice"
  fixSetsAll(statesToSave);
  // Remove newly create duplicates
  removeDuplicates(statesToSave);

  // Print WARD allocations percentage
  processStatesToSave(*statesToSave);

  // Clean memory
  delete statesToSave;

  return 0;
}
