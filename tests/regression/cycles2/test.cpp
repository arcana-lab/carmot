#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "../../../include/wrapper.hpp"

extern void GenerateConnectionGraph();
extern void findCyclesInConnectionGraph();
extern void ReportStatistics();

volatile int* alloc01  [4096];
volatile int* alloc02  [4096];
volatile int* alloc03  [4096];
volatile int* alloc04  [4096];
volatile int* alloc05  [4096];
volatile int* alloc06  [4096];
volatile int* alloc07  [4096];
volatile int* alloc08  [4096];
volatile int* alloc09  [4096];
volatile int* alloc4096 [4096];
volatile int* alloc11  [4096];



int main(int argc, char** argv){
  uint64_t stateID = caratGetStateWrapper("main", 0);

  alloc01[0] = (int*)&(alloc02[0]);
  alloc02[0] = (int*)&(alloc03[0]);
  alloc03[0] = (int*)&(alloc04[0]);
  alloc04[0] = (int*)&(alloc05[0]);
  alloc05[0] = (int*)&(alloc06[0]);
  alloc06[0] = (int*)&(alloc07[0]);
  alloc07[0] = (int*)&(alloc08[0]);
  alloc08[0] = (int*)&(alloc09[0]);
  alloc09[0] = (int*)&(alloc4096[0]);
  alloc4096[0] = (int*)&(alloc11[0]);
  alloc11[0] = (int*)&(alloc01[0]);

  printf("Should be making a conenction via %p for allocs %p to %p\n", &(alloc4096[1]), alloc4096, alloc11);
  printf("Cycle should be %p->%p->%p->%p->%p->%p->%p->%p->%p->%p->%p\n", alloc01, alloc02, alloc03, alloc04, alloc05, alloc06, alloc07, alloc08, alloc09, alloc4096, alloc11);

  caratReportStateWrapper(stateID);


  return 0;
}
