#include <stdlib.h>
#include <stdint.h>
#include<time.h> 

extern void GenerateConnectionGraph();
extern void findCyclesInConnectionGraph();


int main(int argc, char** argv){
  
    int numAllocs = 1000;

    if(argc > 1){
        numAllocs = atoi(argv[1]);
    } 
    
    volatile int*** allocs = (volatile int***)malloc(sizeof(volatile int**)*numAllocs);


    //allocate a bunch of small allocs
    for(int i = 0; i < numAllocs; i++){
        allocs[i] = (volatile int**) malloc(sizeof(volatile int*) * 10);
    }
   
    int seed = 0;

    if(argc > 2){
       seed = atoi(argv[2]); 
    }
     
    srand(seed);

    //randomly connect the allocations
    for(int i = 0; i < (numAllocs*10); i++){
        int from = rand() % numAllocs;
        int to   = rand() % numAllocs;
        allocs[from][1] = (volatile int*)(&(allocs[to][1])); 
    }

    GenerateConnectionGraph();
    findCyclesInConnectionGraph();
    

}
