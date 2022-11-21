//This test will:
//1) make a simple state
//2) add a single pointer to it
//3) complete the state
//4) print out the state

#include <stdlib.h>
#include <stdint.h>
extern "C" uint64_t CGetState(char* fN, uint64_t lN, uint64_t temporalTracking);
extern "C" uint64_t CEndState(uint64_t sID);


int main(int argc, char** argv){
    uint64_t state = CGetState("main", 14, 1);
    int* a = (int*) malloc(512);
    a[5] = 420;
    CEndState(state);
}
