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

  std::string res = "";
  for (auto i = 0; i < nptrs; ++i) {
    std::stringstream ss;
    ss << buffer[i];
    std::string r = sh("addr2line -e " + exec_path + " -p -f -C " + ss.str());
    if (r.size() <= 0){
      continue;
    }
    if (r.at(0) == '?'){
      continue;
    }
    res += "\t\t" + r;
  }

  return res;
}

void copyCallstack(void *buffer[TRACE_SIZE], Trace *trace){
  for (auto i = 0; i < trace->nptrs; ++i){
    buffer[i] = (void*) (trace->buffer[i] - 1); // -1 because we we want the instruction pointer address of the instruction before, which is the actual call, not the return address
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


