#include <stdint.h>
#include <stdlib.h>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <set>
#include "runtime_tables.hpp"

extern "C" void user_init();

extern allocEntry* StackEntry;


struct patch_item{
    void* paddr_of_ptr;
    void* new_paddr_to_write;
};

struct patchlist{
    uint64_t count = 0;
    patch_item* patches;
};

union ptrComps{
    void* ptr;
    uint64_t val;
    int64_t sval;
};

typedef struct {
    void* oldAddr;
    void* newAddr;
    uint64_t length;
} regions;


typedef enum{
    PAGE_4K_GRANULARITY,
    ALLOCATION_GRANULARITY,
} granularity_t;

extern "C" int patching_entry(void**, regions*, uint64_t*, granularity_t);

extern "C" int safe_to_patch(void *begin);



//Executes patch prepared by carat_prepare_patch
void execute_patch(patchlist*);

//This will calloc all the new memory spots
bool verifyPatch(std::unordered_set<allocEntry*>* allocs);

//Prepares patching for memory
void* prepare_patch(void**, std::unordered_set<allocEntry*>, granularity_t);

