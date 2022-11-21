#include <stdlib.h>
#include <stdint.h>
extern void GenerateConnectionGraph();
extern void findCyclesInConnectionGraph();


int main(int argc, char** argv){


  volatile int** alloc01 = (volatile int**)malloc(sizeof(int*)*10);
  volatile int** alloc02 = (volatile int**)malloc(sizeof(int*)*10);
  volatile int** alloc03 = (volatile int**)malloc(sizeof(int*)*10);
  volatile int** alloc04 = (volatile int**)malloc(sizeof(int*)*10);
  volatile int** alloc05 = (volatile int**)malloc(sizeof(int*)*10);
  volatile int** alloc06 = (volatile int**)malloc(sizeof(int*)*10);
  volatile int** alloc07 = (volatile int**)malloc(sizeof(int*)*10);
  volatile int** alloc08 = (volatile int**)malloc(sizeof(int*)*10);
  volatile int** alloc09 = (volatile int**)malloc(sizeof(int*)*10);
  volatile int** alloc10 = (volatile int**)malloc(sizeof(int*)*10);
  volatile int** alloc11 = (volatile int**)malloc(sizeof(int*)*10);


  alloc01[1] = (int*)&(alloc02[1]);
  alloc02[1] = (int*)&(alloc03[1]);
  alloc03[1] = (int*)&(alloc04[1]);
  alloc04[1] = (int*)&(alloc05[1]);
  alloc05[1] = (int*)&(alloc06[1]);
  alloc06[1] = (int*)&(alloc07[1]);
  alloc07[1] = (int*)&(alloc08[1]);
  alloc08[1] = (int*)&(alloc09[1]);
  alloc09[1] = (int*)&(alloc10[1]);
  alloc10[1] = (int*)&(alloc11[1]);
  alloc11[1] = (int*)&(alloc01[1]);


  GenerateConnectionGraph();
  findCyclesInConnectionGraph();


}
