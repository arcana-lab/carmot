#include "help.hpp"

std::string execPathGlobal;

std::string sh(std::string cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe) throw std::runtime_error("popen() failed!");
  while (!feof(pipe.get())) {
    if (fgets(buffer.data(), 128, pipe.get()) != nullptr) {
      result += buffer.data();
    }
  }
  return result;
}

std::string print_backtrace(void *buffer[TRACE_SIZE], int nptrs, std::string exec_path) {
  std::stringstream ss;
  for (auto i = 0; i < nptrs; ++i) {
    ss << buffer[i] << " " ;
  }

  std::string addrs = ss.str();
  std::string r = sh("addr2line -e " + exec_path + " -f -C " + addrs);

  return r;
}

void copyCallstack(void *buffer[TRACE_SIZE], Trace *trace){
  for (auto i = 0; i < trace->nptrs; ++i){
    buffer[i] = (void*) trace->buffer[i];
  }

  return;
}

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
  std::cout << "Allocations to clone:\n";
  printStateSetToSave(stateToSave.CO, debugData, "Set CO");
  printStateSetToSave(stateToSave.CIO, debugData, "Set CIO");

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

std::unordered_map<uint64_t, DebugData*>* getDebugData(void){
  std::unordered_map<uint64_t, DebugData*> *debugData = new std::unordered_map<uint64_t, DebugData*>();

  // Read data and build structures
  std::ifstream rfDebugData("debugData.dat", std::ios::in | std::ios::binary);
  if(!rfDebugData) {
    std::cerr << "Cannot open file!" << std::endl;
    abort();
  }

  uint64_t staticID = 1; // 0 is a reserved value;
  while (true){
    DebugData *debugDataTemp = new DebugData();
    rfDebugData.read((char*) debugDataTemp, sizeof(DebugData));
    if (rfDebugData.eof()){
      break;
    }
    (*debugData)[staticID] = debugDataTemp;
    staticID += 1;
  }

  return debugData;
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
