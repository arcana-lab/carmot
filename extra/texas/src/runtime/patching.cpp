#include "patching.hpp"

class texasPatchingStartup{
    public:
        patchingStartup(){
            
            user_init();
        }
} __texasPatchingStartup;



void execute_patch(patchlist* patches){
    //This becomes super parallel with an omp for
    for(int i = 0; i < patches->count; i++){
        if(patches->patches[i].paddr_of_ptr != 0 && patches->patches[i].new_paddr_to_write != 0){ 
            if (safe_to_patch((void*)(((uint64_t*)(patches->patches[i].paddr_of_ptr))[0]))) {
                ((uint64_t*)(patches->patches[i].paddr_of_ptr))[0] = (uint64_t)(patches->patches[i].new_paddr_to_write);
            }
        }
    }
}

bool verifyPatch(std::unordered_set<allocEntry*>* allocsToMove){
    //Currently there is no verification method, just accept all

    for(auto alloc : *allocsToMove){
        if(alloc->length == 0){
            alloc->length += 8;
        }
        alloc->patchPointer = calloc(alloc->length, 1);
    }

    return true;

}


bool doRangesIntersect(void* range1Start, void* range1End, void* range2Start, void* range2End){
    if(range1Start <= range2End && range2Start <= range1End){
        return true;
    }
    return false;
}

std::unordered_set<void*> pagesBeingMoved;
std::unordered_set<void*> pagesToAnalyze;


void addByPage(void* oldAddr, std::unordered_set<allocEntry*>* allocsToMove){

    for (auto a : *allocationMap){
        if(allocsToMove->find(a.second) != allocsToMove->end()){
            continue;
        }
        //cout << "Looking at alloc: " << a.first << "\n";
        if( doRangesIntersect(oldAddr, ((void*)((uint64_t)oldAddr + 4096)), a.first, ((void*)((uint64_t)a.first + a.second->length)) ) ){
            //cout << "They intersect!\n";
            void* newPageToAdd = (void*)(((uint64_t)a.first) & 0xFFFFFFFFFFFFF000);
            if(pagesBeingMoved.find(newPageToAdd) == pagesBeingMoved.end()){
                //cout << "Adding beginning\n";
                pagesToAnalyze.insert(newPageToAdd);
            }
            newPageToAdd = (void*)(((uint64_t)a.first + a.second->length) & 0xFFFFFFFFFFFFF000);
            if(pagesBeingMoved.find(newPageToAdd) == pagesBeingMoved.end()){
                //cout << "Adding end\n";
                pagesToAnalyze.insert(newPageToAdd);
            }

            allocsToMove->insert(a.second);
        }

    }

}


patchlist* prepare_patch(void** oldAddr, std::unordered_set<allocEntry*>* allocsToMove, granularity_t granularity)
{

    //TODO BARRIER

    auto prospective = allocationMap->find(oldAddr[0]);
    if(prospective == allocationMap->end()){
        return nullptr;
    }
    allocsToMove->insert(prospective->second);

    //Verify the move will progress forward
    if(oldAddr[1] == 0){
        if(!verifyPatch(allocsToMove)){
            //We couldn't alloc the new space
            return nullptr;
        }
    }


    //First allocate up to the amount of patches we will need. Currently, we are
    //being conservative and will alloc a chunk that will guarentee coverage.
    //If we have escape_save this becomes much easier to do

    //Assume that movements will be pointing to start of objects 
    //(Can remove assumption later if we want by page, but that is lame and for nerds)
    uint64_t approxPatchSize = 0;
    for(auto curAddr : *allocsToMove){
        auto potentialEscapingVars = curAddr->allocToEscapeMap;
        approxPatchSize += potentialEscapingVars.size();
    }

    struct patch_item* patches = (struct patch_item*) calloc(sizeof(struct patch_item), approxPatchSize);
    struct patchlist* escapePatches = (struct patchlist*) calloc(sizeof(struct patchlist), 1);

    escapePatches->patches = patches;

    //cout << "Built space for patches\n";
    //Endif thread 0


    /*
     *This section starting now is parallelizable.
     *Each for loop contains independent operations
     *In the future, patchList->count will have to be an atomic increment and get old val.
     */
    //TODO BARRIER, Now also split the potential escaping vars between all the threads

    //We are given, hopefully, a short list of void* that at one time held an alias of allocAddr
    for(auto curAlloc : *allocsToMove){
        if(curAlloc->allocToEscapeMap.size() == 0){
            continue;
        }

        for(auto a : curAlloc->allocToEscapeMap){
            int64_t offset = doesItAlias(curAlloc->pointer, curAlloc->length, ((uint64_t*)(a))[0]);
            if(offset > 0){
                //escapeVar needs to be patched
                struct patch_item curPatch;
                //Write where patch exists
                curPatch.paddr_of_ptr = a;
                //Write what we will need to write there
                curPatch.new_paddr_to_write = (void*)((uint64_t)(curAlloc->patchPointer) + offset);
                escapePatches->patches[escapePatches->count] = curPatch;
                escapePatches->count++;
            }
        }
    }

    //Time to update the allocationMap
    for(auto allocEntry : *allocsToMove) {
        if (safe_to_patch(allocEntry->pointer)) {
            auto a = allocationMap->extract(allocEntry->pointer);
            //Allocation key doesn't work
            if(a.empty()){
                continue;
            }
            a.key() = allocEntry->patchPointer; 
            allocEntry->pointer = allocEntry->patchPointer;
            allocationMap->insert(move(a));
        }
    }

    return escapePatches;
}



extern "C" int texas_entry(void **addrs, regions* patchedAllocs, uint64_t* numAllocs, granularity_t granularity){
    //cout << "Entered carat_entry\n";
    std::unordered_set<allocEntry*>* allocsToMove = new std::unordered_set<allocEntry*>; 

    //This will make a patchList and also 
    patchlist* escapePatches = nullptr;
    escapePatches = prepare_patch(addrs, allocsToMove, granularity);
    //cout << "Returned from carat_prepare_patch\n";
    if(escapePatches == nullptr){
        //cout << "No patches to make\n";
        return 0;
    }

    //Execute patches in memory
    execute_patch(escapePatches);

    //This will make an array of every alloc that will be moved along with the new address
    *numAllocs = allocsToMove->size();

    patchedAllocs = (regions *) malloc(sizeof(regions) * (*numAllocs));
    //Now create allocRegions to return for signaller
    int i = 0;
    for(auto a : *allocsToMove){
        patchedAllocs[i].oldAddr = a->pointer;
        patchedAllocs[i].newAddr = a->patchPointer;
        patchedAllocs[i].length = a->length;    
        i++;
    }

    return 0;
}



