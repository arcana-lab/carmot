#include <stdlib.h>
//#include <stdio.h>

unsigned long func(int arg){
  //std::cerr << "printing: " << arg << "\n";

  int* a;
  unsigned long b;

  a = (int*) malloc(sizeof(int)); 
  //printf("Address of a: %lx\n", &a);

  //*a = arg;
  *a = 5;
  b = (unsigned long) a;
  free(a);

  return b;
}
