#include "utils.hpp"
#include "DebugData.hpp"

#include <regex>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

#include <execinfo.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <zconf.h>

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
  std::string pathToFile = debugData->pathToFile;
  unsigned lineNum = debugData->lineNum;
  unsigned columnNum = debugData->columnNum;
  std::fstream infile;
  infile.open(pathToFile, std::ios::in);
  if (!infile.is_open()){
    std::cerr << "ERROR: file " << pathToFile << " not found. Abort.\n";
    abort();
  }

  //std::regex regexp("[_A-Za-z0-9]+"); // Variable names in C++
  std::regex regexp("[_A-Za-z]"); // First char of a variable name in C++
  std::smatch match;
  std::string line;
  auto currLineNum = 1;
  while (std::getline(infile, line)){
    if (currLineNum == lineNum){
      std::regex_search(line, match, regexp);
      break;
    }
    currLineNum += 1;
  }
  infile.close();

  std::string columnChar = std::string(1, line[columnNum-1]);

  // Print line
  std::cout << currLineNum << " " << columnNum << " " << columnChar << " " << line << "\n";

  // Print callstack
  if (trace == nullptr){
    return;
  }
  void *buffer[TRACE_SIZE];
  copyCallstack(buffer, trace);
  std::string back_trace = print_backtrace(buffer, trace->nptrs, execPathGlobal);
  std::cout << back_trace << "\n";

  return;
}

void printUseToSave(UseToSave &useToSave, std::unordered_map<uint64_t, DebugData*> &debugData){
  Trace *trace = useToSave.trace;
  uint64_t staticID = useToSave.staticID;
  if (debugData.count(staticID) == 0){
    std::cerr << "ERROR: staticID " << staticID << " not found in debugData. Abort.\n";
    abort();
  }
  DebugData *debugDatum = debugData[staticID];
  printDebugData(debugDatum, trace);

  return;
}

void printUsesToSave(std::vector<UseToSave> &usesToSave, std::unordered_map<uint64_t, DebugData*> &debugData){
  for (auto &useToSave : usesToSave){
    printUseToSave(useToSave, debugData);
  }

  return;
}

void printAllocationToSave(AllocationToSave &allocationToSave,  std::unordered_map<uint64_t, DebugData*> &debugData){
  Trace *trace = allocationToSave.trace;
  uint64_t staticID = allocationToSave.staticID;
  if (debugData.count(staticID) == 0){
    std::cerr << "ERROR: staticID " << staticID << " not found in debugData. Abort.\n";
    abort();
  }
  printUsesToSave(allocationToSave.uses, debugData);

  return;
}

void printStateSetToSave(StateSetToSave &stateSetToSave,  std::unordered_map<uint64_t, DebugData*> &debugData, std::string setName){
  if (!setName.empty()){
    std::cout << setName << "\n";
  }
  for (auto &alloc : stateSetToSave.allocations){
    printAllocationToSave(alloc, debugData);
  }

  return;
}

void printStateToSave(StateToSave &stateToSave,  std::unordered_map<uint64_t, DebugData*> &debugData){
  std::cout << "To variables\n";
  printStateSetToSave(stateToSave.I, debugData, "");
  printStateSetToSave(stateToSave.CIO, debugData, "");
  printStateSetToSave(stateToSave.TIO, debugData, "");

  std::cout << "FromTo variables\n";
  printStateSetToSave(stateToSave.TO, debugData, "");
  printStateSetToSave(stateToSave.TIO, debugData, "");

  std::cout << "From variables\n";
  printStateSetToSave(stateToSave.O, debugData, "");
  printStateSetToSave(stateToSave.CO, debugData, "");
  printStateSetToSave(stateToSave.TO, debugData, "");
  printStateSetToSave(stateToSave.IO, debugData, "");
  printStateSetToSave(stateToSave.CIO, debugData, "");
  printStateSetToSave(stateToSave.TIO, debugData, "");

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

StatesToSave* getRunData(void){
  StatesToSave *statesToSave = new StatesToSave();

  // Create and input archive
  std::ifstream ifs("runData.dat");
  boost::archive::binary_iarchive ar(ifs);

  // Load the data
  ar & (*statesToSave);

  return statesToSave;
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
