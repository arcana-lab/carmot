//This program is intended to test multiple types of data structures for potential optimization.
#include "dataStructureTester.hpp"


char* implementation = "";
char* logFile = "";
char* dataFile = "";
uint64_t numNodes = 0;
uint64_t numLookups = 0;

uint64_t *keys;
uint64_t *lookups;


int main (int argc, char** argv){
    //First thing we do is figure out which data structure is underneath us
    if(argc < 6){
        std::cout << "Arguments are as follows: binary implementation numNodes numLookups File LogFile\n";
        std::cout << "Available implementations are:\n";
        for(auto &[name, val] : implementations){
            std::cout << name << "\n";
        }
        return 1;
    }

    implementation = argv[1];
    sscanf(argv[2], "%lu", &numNodes);
    sscanf(argv[3], "%lu", &numLookups);
    dataFile = argv[4];
    logFile = argv[5];

    //Will default to splay tree
    uint64_t impl = 0;
    if(implementations.find(implementation) != implementations.end()){
        impl = implementations[implementation];
    }
    else{
        std::cout << "Invalid Implementation!!!\n";
    }
    std::cout << "Arguments are as follows:\n" 
        << "Implementation: " << implementation 
        << "\nNumber of Nodes: " << numNodes
        << "\nNumber of Lookups: " << numLookups << "\n";

    dataStructure* tree = new dataStructure(impl);
    keys = (uint64_t*) malloc(numNodes*sizeof(long));
    lookups= (uint64_t*) malloc(numLookups*sizeof(long));



    //Open data file and populate keys and lookups
    FILE* dataFILE = fopen(dataFile, "r+");

    for (int i=0;i<numNodes;i++) {
        if (fscanf(dataFILE, "insert %lu\n",&keys[i])!=1) {
            fprintf(stderr,"failed to read insert %lu\n",i);
            exit(-1);
        }
    }

    for (int i=0;i<numLookups;i++) {
        if (fscanf(dataFILE, "lookup %lu\n",&lookups[i])!=1) {
            fprintf(stderr,"failed to read lookup %lu\n",i);
            exit(-1);
        }
    }


    //Insert the nodes

    for (int i=0;i<numNodes;i++) { 
        tree->insert(keys[i]);         
        printf("Node %d of %d\r", i+1, numNodes);
    }

    printf("\n%lu keys inserted\n",numNodes);


    //Search through the nodes

    for (int i=0;i<numLookups;i++) { 
        void* test = tree->find(lookups[i]);
        printf("Lookup %d of %d\r", i+1, numLookups);
    }
    std::cout << "\n";




}
