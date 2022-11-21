#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "hashMap.hpp"

extern uint64_t CGetState(char*, uint64_t, uint64_t);
extern  uint64_t CEndState(uint64_t);



#define LOGGING 1
#define mallocz(n) memset(malloc(n),0,n)

unsigned long num_keys;
unsigned long num_lookups;
long *keys;
long *lookups;



FILE* loggingFile = NULL;

#if LOGGING

void openLogging(){
	loggingFile = fopen("splayNodes.log", "w");

}

void closeLogging(){
	fclose(loggingFile);	
}

#else

void* superMalloc = NULL;
void** offsets = NULL;
void openLogging(){
	loggingFile = fopen("splayNodes.opt", "r");
	uint64_t superSize;
	uint64_t numMallocs;
	fscanf(loggingFile, "%d %d", &superSize, &numMallocs);
	superMalloc = mallocz(superSize);
	offsets = mallocz(numMallocs*sizeof(void*));
	uint64_t i = 0;
	uint64_t offset = NULL;
	while(fscanf(loggingFile, "%d", &offset) == 1){
		offsets[i] = &(superMalloc[offset]);
		i++;
	}
	fclose(loggingFile);
}

void closeLogging(){	
	free(superMalloc);
	free(offsets);
}

#endif


void print(splay_tree_key k, void *state)
{
    printf("%ld\n",k->key);
}



void* mallocLogger(uint64_t n, uint64_t i){
	void* retVal = NULL;

#if LOGGING
	retVal = mallocz(n);
	fprintf(loggingFile, "%p\n", retVal);
    CAddToState(retVal, 1);
#else
	retVal = offsets[i];
#endif //LOGGING	
	return retVal;
}

int main(int argc, char *argv[])
{
  openLogging();
  unsigned long i;
  

  if (argc<3) {
    fprintf(stderr,"usage: run-splay numkeys numlookups < stream\n");
    exit(-1);
  }


  num_keys=atol(argv[1]);
  num_lookups=atol(argv[2]);

  keys = (long*) malloc(num_keys*sizeof(long));
  lookups= (long*) malloc(num_lookups*sizeof(long));

  for (i=0;i<num_keys;i++) {
    if (scanf("insert %ld\n",&keys[i])!=1) {
      fprintf(stderr,"failed to read insert %lu\n",i);
      exit(-1);
    }
  }
  
  for (i=0;i<num_lookups;i++) {
    if (scanf("lookup %ld\n",&lookups[i])!=1) {
      fprintf(stderr,"failed to read lookup %lu\n",i);
      exit(-1);
    }
  }

  printf("data loads done\n");
  uint64_t allocID = CGetState("main", 128, 1);

  splay_tree s = (splay_tree) mallocz(sizeof(*s));
  

  for (i=0;i<num_keys;i++) { 
    splay_tree_node n = (splay_tree_node) mallocLogger(sizeof(*n), i);
    n->key.key=keys[i];
    splay_tree_insert(s,n);
    printf("Node %d of %d\n", i+1, num_keys);
  }

  printf("%lu keys inserted\n",num_keys);
  

  struct splay_tree_key_s lookup_key;

  for (i=0;i<num_lookups;i++) { 
    lookup_key.key = lookups[i];
    volatile splay_tree_key result = splay_tree_lookup(s,&lookup_key);
    if(i % 1000 == 0){
        printf("Lookup %d of %d\r", i, num_lookups);
    }
  }
  printf("\n");

  CEndState(allocID);


  printf("%lu lookups done\n",num_lookups);

  //  splay_tree_foreach(s,print,0);
  closeLogging();

}
