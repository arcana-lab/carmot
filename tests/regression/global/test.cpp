#include <cstdint>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <fstream>
#include "../../../include//wrapper.hpp"

class C{
public:
  int field;
  int *mfield = nullptr;

  C(std::string pathToFile){
    std::ifstream f;
    f.open(pathToFile);
    int size = 0;
    for(std::string line; getline(f, line); ){
      size = std::stoi(line);
    }
    f.close();

    this->mfield = (int*) malloc(size*sizeof(int));
  }
};

int a = 234; int e = 5;
float b = 0.12;
double c = 0.45;
int64_t d = 123;
int *pointer = nullptr;
int *m = (int*) malloc(3*sizeof(int));
C *obj = new C("input.txt");

int main (int argc, char *argv[]){
  uint64_t stateID = caratGetStateWrapper((char*)"main", 0);
  a = 235;
  e = 6;
  b = 0.13;
  c = 0.46;
  d = 124;
  m[0] = 1;
  obj->field = 2;
  pointer = nullptr;
  caratReportStateWrapper(stateID);

  return 0;
}
