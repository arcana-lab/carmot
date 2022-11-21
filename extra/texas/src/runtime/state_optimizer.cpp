

#include "state_optimizer.hpp"


//This function will optimize the given state
void OptimizeState(uint64_t stateID, hueristic_t hueristic){
    std::vector<allocEntry*>* packedAllocOrder = new std::vector<allocEntry*>();
    GetOptimizedState(stateID, hueristic, packedAllocOrder);

    std::cout << "Starting packer state\n";
    struct state_packer_state* packingState; 
    uint64_t alignment = ALIGNMENT_SIZE;
    uint64_t cache_line_size = CACHE_LINE_SIZE;
    packingState = state_packer_new(alignment, cache_line_size);
    int layout = state_packer_start_layout(packingState);
   
    std::cout << "Adding in packed order to state\n"; 
    //Add in order via the packing interface (state_packer.h)
    for(auto a : *packedAllocOrder){
        layout = state_packer_add_allocation(packingState, a->pointer, a->length, a->alignment, &(a->patchPointer));
    }

#if ONLINE    
    //Indicate to the packer that it is time to move
    state_packer_start_move(packingState);

    //Actually move the allocs to the new super-alloc
    for (auto a : *packedAllocOrder){
        layout = state_packer_move_allocation(packingState, a->pointer, a->length, &(a->patchPointer));
    }

    //Indicate we are done
    layout = state_packer_finish(packingState);
   
    //Optionally free the packingState???
    //layout = state_packer_free(packingState);

#else //OFFLINE
    std::cout << "Opening file to write out state\n";
    std::ofstream myfile;
    std::stringstream ss; 
    ss << "state" << stateID << ".packing";
    std::string fileToOpen = ss.str();
    myfile.open(fileToOpen);
    myfile << std::hex << "0x" << get_current_offset(packingState) << "\n";
    for(auto a : *packedAllocOrder){
        myfile << std::hex << "0x" << (uint64_t)a->pointer << "\t" << (uint64_t)a->patchPointer << "\n";
    }
    myfile.close();

#endif //ONLINE

 
}

//This function will fill a vector with the order to pack the given state
void GetOptimizedState(uint64_t stateID, hueristic_t hueristic, std::vector<allocEntry*>* packedAllocs){
    auto entry = stateMap.find(stateID);
    if(entry == stateMap.end()){
        return;
    }
    auto state = entry->second;
    if(state->allocations->size() < 2){
        return;
    }

    GenerateConnectionGraph();
    state->processStateWindow();

    //TODO
    //struct state_packer_state * result = state_packer_new(4KB, 64);
    //int res = state_packer_start_layout(result);
    allocEntry* firstNode = nullptr;
    allocEntry* secondNode = nullptr;
    std::unordered_set<allocEntry*> unplacedAllocs;
    for(auto a : *(state->allocations)){
        unplacedAllocs.insert(a);
    }
    if(unplacedAllocs.find(StackEntry) != unplacedAllocs.end()){
        unplacedAllocs.erase(StackEntry);
    }

   /*
       TEMPORAL_HEAVIEST_EDGE_FIRST = 0,
       POINTER_HEAVIEST_EDGE_FIRST,
       TEMPORAL_HEAVIEST_NODE_FIRST,
       POINTER_HEAVIEST_NODE_FIRST,
    */
    switch(hueristic){
        case TEMPORAL_HEAVIEST_EDGE_FIRST:
            TemporalHeaviestEdgeFirst(state, &unplacedAllocs, packedAllocs);
            break;

        case TEMPORAL_HEAVIEST_NODE_FIRST:

            break;

        default:
            TemporalHeaviestEdgeFirst(state, &unplacedAllocs, packedAllocs);

    }


    bool expectedPath = true;
    //This loop will run at least one iteration because we make sure that we have at least two allocations to start
    //This loop will generate (in almost a linked-list-like fashion) the order that we will pack the nodes
    
    std::cout << "Returning with completed packedAllocs\n";
    return;

}


//This method will sort the nodes by starting with a node with the heaviest edge, then selecting the next node based off of which node has the highest weight with it.
void TemporalHeaviestEdgeFirst(state* state, std::unordered_set<allocEntry*>* unplacedAllocs, std::vector<allocEntry*>* packedAllocs){
    allocEntry* currentNode = nullptr;
    allocEntry* nextNode = nullptr;
    while(unplacedAllocs->size() > 0){
        if(unplacedAllocs->size() % 1000 == 0){ 
            std::cout << "CurSize: " << unplacedAllocs->size() << "\n";
        }
        nextNode = nullptr;
        uint64_t weight = 0;
        //We must find a new node to start with
        if(currentNode == nullptr){
            //std::cout << "No currentNode\n";
            for(auto candidateNode : *unplacedAllocs){
                auto candidateTemporal = state->temporalConnections.find(candidateNode);
                for(auto tempNode : *(candidateTemporal->second)){
                    if((unplacedAllocs->find(tempNode.first) != unplacedAllocs->end()) && (tempNode.first != candidateNode)){
                        if(weight < tempNode.second){
                            weight = tempNode.second;
                            currentNode = candidateNode;
                            nextNode = tempNode.first;
                        }
                    }
                }
            }
        }
        //This means the ordering doesn't matter anymore since there is no more significant temporal edges
        //among the remaining allocations
        if(currentNode == nullptr){
            //std::cout << "Quick control flow break\n";
            for(auto candidateNode : *unplacedAllocs){
                packedAllocs->push_back(candidateNode);
            }
            unplacedAllocs->clear();
            return;
        }
        //This means we have a current node, and we now must get a nextNode
        else if(nextNode == nullptr){
            //std::cout << "Finding next node\n";
            auto candidateTemporal = state->temporalConnections.find(currentNode);
            for(auto tempNode : *(candidateTemporal->second)){
                if((unplacedAllocs->find(tempNode.first) != unplacedAllocs->end()) && (tempNode.first != currentNode)){
                    if(weight < tempNode.second){
                        weight = tempNode.second;
                        nextNode = tempNode.first;
                    }
                }

            }
        }

        //std::cout << "Done with iter\n";
        //We should now have a currentNode and possibly a next node
        packedAllocs->push_back(currentNode);
        unplacedAllocs->erase(currentNode);
        currentNode = nextNode;

    }
    
}



void OptimizeStates(){
    for(auto state : stateMap){
        OptimizeState(state.second->stateID, TEMPORAL_HEAVIEST_EDGE_FIRST);
    }
}
