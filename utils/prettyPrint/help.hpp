#pragma once

#include "utils.hpp"
#include "DebugData.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <memory>

#include <execinfo.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <zconf.h>

extern std::string execPathGlobal;

std::string print_backtrace(void *buffer[TRACE_SIZE], int nptrs, std::string exec_path);
std::unordered_map<uint64_t, DebugData*>* getDebugData(void);
StatesToSave* getRunData(void);
void copyCallstack(void *buffer[TRACE_SIZE], Trace *trace);
