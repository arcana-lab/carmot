#pragma once

#ifndef DEBUG_DATA
#define DEBUG_DATA

#include <string>
#include <cstring>
#include <iostream>

#define STRING_SIZE 1000

class DebugData{
  public:
  char pathToFile[STRING_SIZE];
  char varName[STRING_SIZE];
  unsigned lineNum = 0;
  unsigned columnNum = 0;

  DebugData(){} ;
  DebugData(std::string pathToFile, std::string varName, unsigned lineNum, unsigned columnNum):lineNum{lineNum},columnNum{columnNum}{
    auto pathToFileSize = pathToFile.size();
    auto varNameSize = varName.size();
    if ((pathToFileSize >= STRING_SIZE) || (varNameSize >= STRING_SIZE)){
      std::cerr << "ERROR: STRING_SIZE = " << STRING_SIZE << " but pathToFile.size() = " << pathToFileSize << " and varName = " << varNameSize << " . Abort.\n";
      abort();
    }

    // Set char[] to 0 to avoid valgrind uninitialized bytes error.
    memset(this->pathToFile, 0, STRING_SIZE);
    memset(this->varName, 0, STRING_SIZE);

    memcpy(this->pathToFile, pathToFile.c_str(), pathToFile.size() + 1);
    memcpy(this->varName, varName.c_str(), varName.size() + 1);
  }

};

#endif
