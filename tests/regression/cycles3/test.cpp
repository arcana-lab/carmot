#include <stdlib.h>
#include <stdint.h>
#include "../../../include/wrapper.hpp"

extern void GenerateConnectionGraph();
extern void findCyclesInConnectionGraph();
extern void ReportStatistics();


int main(int argc, char** argv){
	volatile int** alloc01 = (volatile int**)malloc(sizeof(int*)*4096);
	volatile int** alloc02 = (volatile int**)malloc(sizeof(int*)*4096);
	volatile int** alloc03 = (volatile int**)malloc(sizeof(int*)*4096);
	volatile int** alloc04 = (volatile int**)malloc(sizeof(int*)*4096);
	volatile int** alloc05 = (volatile int**)malloc(sizeof(int*)*4096);
	volatile int** alloc06 = (volatile int**)malloc(sizeof(int*)*4096);
	volatile int** alloc07 = (volatile int**)malloc(sizeof(int*)*4096);
	volatile int** alloc08 = (volatile int**)malloc(sizeof(int*)*4096);
	volatile int** alloc09 = (volatile int**)malloc(sizeof(int*)*4096);
	volatile int** alloc4096 = (volatile int**)malloc(sizeof(int*)*10);
	volatile int** alloc11 = (volatile int**)malloc(sizeof(int*)*4096);


  alloc01[1] = (int*)&(alloc02[1]);
	alloc02[1] = (int*)&(alloc03[1]);
	alloc03[1] = (int*)&(alloc04[1]);
	alloc04[1] = (int*)&(alloc05[1]);
	alloc05[1] = (int*)&(alloc06[1]);
	alloc06[1] = (int*)&(alloc07[1]);
	alloc07[1] = (int*)&(alloc08[1]);
	alloc08[1] = (int*)&(alloc09[1]);
	alloc09[1] = (int*)&(alloc4096[1]);
	alloc4096[1] = (int*)&(alloc11[1]);
	alloc11[1] = (int*)&(alloc01[1]);

  uint64_t stateID = caratGetStateWrapper("main", 0);

  //Generate a second cycle
  volatile int** balloc01 = (volatile int**)malloc(sizeof(int*)*4096);
  volatile int** balloc02 = (volatile int**)malloc(sizeof(int*)*4096);
  volatile int** balloc03 = (volatile int**)malloc(sizeof(int*)*4096);
  volatile int** balloc04 = (volatile int**)malloc(sizeof(int*)*4096);
  volatile int** balloc05 = (volatile int**)malloc(sizeof(int*)*4096);
  volatile int** balloc06 = (volatile int**)malloc(sizeof(int*)*4096);
  volatile int** balloc07 = (volatile int**)malloc(sizeof(int*)*4096);
  volatile int** balloc08 = (volatile int**)malloc(sizeof(int*)*4096);
  volatile int** balloc09 = (volatile int**)malloc(sizeof(int*)*4096);
  volatile int** balloc4096 = (volatile int**)malloc(sizeof(int*)*4096);
  volatile int** balloc11 = (volatile int**)malloc(sizeof(int*)*4096);


  balloc01[1] = (int*)&(balloc02[1]);
  balloc02[1] = (int*)&(balloc03[1]);
  balloc03[1] = (int*)&(balloc04[1]);
  balloc04[1] = (int*)&(balloc05[1]);
  balloc05[1] = (int*)&(balloc06[1]);
  balloc06[1] = (int*)&(balloc07[1]);
  balloc07[1] = (int*)&(balloc08[1]);
  balloc08[1] = (int*)&(balloc09[1]);
  balloc09[1] = (int*)&(balloc4096[1]);
  balloc4096[1] = (int*)&(balloc11[1]);
  balloc11[1] = (int*)&(balloc01[1]);

  caratReportStateWrapper(stateID);

  return 0;
}
