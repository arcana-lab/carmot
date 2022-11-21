#include <cstdio>
#include <cstdint>
#include <stdlib.h>
#include <sys/mman.h>
#include "../../../include/wrapper.hpp"

class C{
public:
  int a;
  int b;

  C(){
   this->a = 111;
   this->b = 222;
  }
};

// Globals
int global = 3;

int main (int argc, char *argv[]){
  int size = 400000;

  // mmap
  /*
  int fd = open("./test.txt", O_WRONLY);
  int sizemmap = size*sizeof(char);
  char *map = (char*) mmap(nullptr, sizemmap, PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == nullptr){
    std::cerr << "ERROR\n";
    abort();
  }
  map[0] = 'a';
  map[1] = 'b';
  map[2] = 'c';
  map[3] = 'd';
  */

  // malloc
  int *m = (int*) malloc(size*sizeof(int));
  m[0] = 1;
  m[1] = 3;
  m[2] = 73;
  m[3] = 67;

  // calloc
  int *c = (int*) calloc(size + 1, sizeof(int));

  // new
  C *objPtr = new C{};
  void *weirdPtr = reinterpret_cast<void *>(&objPtr);

  // alloca
  C objStack{};

  uint64_t stateID = 0;
  // program section to track
  for (int i = 0; i < (size - 1); ++i){
    stateID = caratGetStateWrapper("main", 0);
    //map[i+1] = map[i];
    m[i+1] = m[0] + m[i];
    c[i+1] = c[i] + m[i];
    C ** objPtrPtr = reinterpret_cast<C **>(weirdPtr);
    (*objPtrPtr)->a = m[i];
    objStack.b = m[i-1];
    global = 4;
    //size = size - 1;
    caratReportStateWrapper(stateID);
  }
  endStateInvocationWrapper(stateID);

  // clean up
  free(c);
  free(m);
  //munmap(map, size);
  //close(fd);

  return 0;
}
