#include "AllocationTable.hpp"
#include <boost/graph/detail/adjacency_list.hpp>
#include <cmath>
#include <iostream>
#include <sstream>
//--------------------CONSTRUCTORS---------------------------
//Default constructor 
AllocationTable::AllocationTable(){
  this->sortedAllocationTable = new AllocationTableType();
  this->hashAllocationTable = new HashAllocationTableType();
  this->allocConnections = new graph_t();
  this->masks = new MasksType();
  this->maskLengths = new MasksType();
}

//This constructor will add the integer to the set of masks
AllocationTable::AllocationTable(uint64_t mask){
  this->sortedAllocationTable = new AllocationTableType();
  this->hashAllocationTable = new HashAllocationTableType();
  this->allocConnections = new graph_t();
  this->masks = new MasksType();
  this->maskLengths = new MasksType();
  this->AddMaskForLength(mask);
}

//This constructor will add the set of integers to become the masks
AllocationTable::AllocationTable(std::unordered_set<uint64_t> masks){
  this->sortedAllocationTable = new AllocationTableType();
  this->hashAllocationTable = new HashAllocationTableType();
  this->allocConnections = new graph_t();
  this->masks = new MasksType();
  this->maskLengths = new MasksType();
  for(auto i : masks){
    this->AddMaskForLength(i);
  }
}


//--------------------FUNCTIONS------------------------------


Allocation* AllocationTable::findAllocation(void* key){
  for(auto mask : *masks){
    auto entry = hashAllocationTable->find(MaskKey(key, mask));
    if(entry != hashAllocationTable->end()){
      return entry->second;
    }
  }
  auto prospective = sortedAllocationTable->lower_bound(key);
  uint64_t blockStart;
  uint64_t blockLen;

  //prospective will be either not found at all, matches addressEscaping exactly, or is next block after addressEscaping      

  //Not found at all
  if(prospective == sortedAllocationTable->end()){
    return nullptr;
  }
  blockStart = (uint64_t)prospective->first;
  blockLen = prospective->second->length;
  //Matches exactly
  if(blockStart == (uint64_t)key){
    //Nothing to do, prospective points to our matching block
  }
  //Could be in previous block
  else if (prospective != sortedAllocationTable->begin()){
    prospective--;
    blockStart = (uint64_t)prospective->first;
    blockLen = prospective->second->length;
    if(doesItAlias(prospective->first, blockLen, (uint64_t)key) == -1){
      //Not found
      return nullptr;
    }
  }
  else{
    return nullptr;
  }
  return prospective->second;

}

void AllocationTable::InsertAllocation(void* key, Allocation* entry){
  auto newTableEntry = std::make_pair(key, entry);

  //Check if allocation belongs in hashMap
  for(auto len : *maskLengths){
    if(len == entry->length){
      //Insert into hash table
      hashAllocationTable->insert(newTableEntry);
      return;
    }
  }
  //Insert into ordered table
  sortedAllocationTable->insert(newTableEntry);
}

void AllocationTable::DeleteAllocation(void* key){
  //Check if allocation exists in hash table
  if(hashAllocationTable->find(key) != hashAllocationTable->end()){
    hashAllocationTable->erase(key); 
    return;
  }
  sortedAllocationTable->erase(key);
}

void AllocationTable::Clear(){
  hashAllocationTable->clear();
  sortedAllocationTable->clear();
}

void AllocationTable::AddMaskForLength(uint64_t length){
  uint64_t mask = -1;
  float shift = log2(length);
  if(ceil(shift) != floor(shift)){
    DEBUG("Cannot add length %lu to hashTable because it is not a power of two!\n", length);
  }
  uint64_t shiftInt = (uint64_t)shift;
  mask = (mask >> shiftInt) << shiftInt;
  masks->insert(mask);
  maskLengths->insert(length);
}

void* AllocationTable::MaskKey(void* ptr, uint64_t mask){
  return ((void*)(((uint64_t)ptr) & mask));
}

void AllocationTable::MergeTable(AllocationTable* tableToMerge, bool overwrite){
  //Merge masks
  this->masks->insert(tableToMerge->masks->begin(),tableToMerge->masks->end());

  if(overwrite){
    //Merge hashmap
    for(auto entry : *(tableToMerge->hashAllocationTable)){
      hashAllocationTable->insert_or_assign(entry.first, entry.second);
    }
    for(auto entry : *(tableToMerge->sortedAllocationTable)){
      //Merge the sortedAllocMaps
      sortedAllocationTable->insert_or_assign(entry.first, entry.second);
    }
  }
  else{
    for(auto entry : *(tableToMerge->hashAllocationTable)){
      hashAllocationTable->insert(entry);
    }
    for(auto entry : *(tableToMerge->sortedAllocationTable)){
      //Merge the sortedAllocMaps
      sortedAllocationTable->insert(entry);
    }
  }
}

void AllocationTable::CommitStateInfoForAllocations(){
  for(auto entry : *hashAllocationTable){
    entry.second->commitStateInfo(nullptr);
  }
  for(auto entry : *sortedAllocationTable){
    entry.second->commitStateInfo(nullptr);
  }
}

//This function will return the size of the allocations in the allocation table (in bytes)
uint64_t AllocationTable::GetMemFootprint(void){
  uint64_t sizeInBytes = 0;
  for(auto entry : *sortedAllocationTable){
    sizeInBytes += entry.second->length;
  }
  for(auto entry : *hashAllocationTable){
    sizeInBytes += entry.second->length;
  }
  return sizeInBytes;
}

void AllocationTable::GenerateConnectionGraph(){
  std::cerr << "Making a connection graph\n";
  //first kill off the old state allocConnections
  std::set<void**> doneEscapes;
  allocConnections->clear();
  //Initialize
  DEBUG("Adding all remaining allocations in allocationMap to connection graph\n");
  for(auto allocs : *sortedAllocationTable){
    DEBUG("Adding %p to graph\n", allocs.second->pointer);
    allocs.second->connectionVertex = boost::add_vertex(Vertex{allocs.second}, *allocConnections);      
  }
  for(auto allocs : *hashAllocationTable){
    DEBUG("Adding %p to graph\n", allocs.second->pointer);
    allocs.second->connectionVertex = boost::add_vertex(Vertex{allocs.second}, *allocConnections);      
  }
  //Iterate through all the allocations of the allocationMap
  for(auto entry : *sortedAllocationTable){
    auto alloc = entry.second;
    //Look at each escape and determine if it falls in another alloc
    for(auto candidateEscape : *(alloc->allocToEscapeMap)){
      //No repetitive work across allocations

      //First verify that it still points to alloc we are dealing with
      if(doesItAlias(alloc->pointer, alloc->length, (uint64_t)(*candidateEscape) ) == -1){
        continue;
      }
      //Now see if the allocation lives in one of the allocationMap entries
      Allocation* pointerAlloc = this->findAllocation((void*)candidateEscape);
      if(pointerAlloc == nullptr){
        continue;
      }
      //We now know that pointerAlloc contains a pointer that points to alloc
      //Now we must modify that allocations allocConnections entry (it also must exist)
      DEBUG("Found an edge connecting alloc %p to %p via escape %p! Adding edge to graph\n", alloc->pointer, pointerAlloc->pointer, candidateEscape);
      boost::add_edge(pointerAlloc->connectionVertex, alloc->connectionVertex, Edge{1}, *allocConnections);
    }
  }
  for(auto entry : *hashAllocationTable){
    auto alloc = entry.second;
    //Look at each escape and determine if it falls in another alloc
    for(void** candidateEscape : *(alloc->allocToEscapeMap)){
      //No repetitive work across allocations

      //First verify that it still points to alloc we are dealing with
      if(doesItAlias(alloc->pointer, alloc->length, (uint64_t)(*candidateEscape) ) == -1){
        continue;
      }
      //Now see if the allocation lives in one of the allocationMap entries
      Allocation* pointerAlloc = findAllocation((void*)candidateEscape);
      if(pointerAlloc == nullptr){
        continue;
      }
      //We now know that pointerAlloc contains a pointer that points to alloc
      //Now we must modify that allocations allocConnections entry (it also must exist)
      DEBUG("Found an edge connecting alloc %p to %p via escape %p! Adding edge to graph\n", alloc->pointer, pointerAlloc->pointer, candidateEscape);
      boost::add_edge(pointerAlloc->connectionVertex, alloc->connectionVertex, Edge{1}, *allocConnections);
    }
  }
}
//This uses the connectionGraph and will print out any cycles it finds.
void AllocationTable::FindCyclesInConnectionGraph(uint64_t stateFilter){

  std::cerr << "Clearing previous cycles\n";
  //First clear out the previous cycles
  for(auto cycle : this->cycles){
    delete cycle;
  }
  this->cycles.clear();
  // typedef graph_traits<adjacency_list<vecS, vecS, directedS> >::vertex_descriptor vertex_t
  std::vector<int> component(num_vertices(*allocConnections)), discover_time(num_vertices(*allocConnections));
  std::vector<vertex_t> root(num_vertices(*allocConnections));
  std::vector<boost::default_color_type> color(num_vertices(*allocConnections)); 
  int num = strong_components(*allocConnections, make_iterator_property_map(component.begin(), boost::get(boost::vertex_index, *allocConnections)),
      root_map(make_iterator_property_map(root.begin(), boost::get(boost::vertex_index, *allocConnections))).
      color_map(make_iterator_property_map(color.begin(), get(boost::vertex_index, *allocConnections))).
      discover_time_map(make_iterator_property_map(discover_time.begin(), get(boost::vertex_index, *allocConnections))));


  std::unordered_map<int, std::vector<int64_t>*> sccToVertexMap;
  std::vector<int>::size_type i;
  for (i = 0; i != component.size(); ++i){
    DEBUG("Vertex %ld is in SCC %d\n", i, component[i]);
    auto verts = sccToVertexMap.find(component[i]);
    std::vector<int64_t>* SCCVec = nullptr;
    if(verts == sccToVertexMap.end()){
      SCCVec = new std::vector<int64_t>();
      sccToVertexMap.insert({component[i], SCCVec});
    }
    else{
      SCCVec = verts->second;
    }
    SCCVec->push_back(i);
  }

  //Now we have a map of all SCCs and the vertices in them
  //Now lets verify the freshness
  for(auto scc : sccToVertexMap){
    //Look for SCCs larger than 1
    if(scc.second->size() > 1){
      std::cerr << "New Cycle Found!\n";
      SmartPointerCycle* newCycle = new SmartPointerCycle();
      //Now we need to verify that the entries are all fresh...
      bool good = false;
      Allocation* oldest = nullptr;
      uint64_t oldVal = -1;
      for(auto vec : *(scc.second)){
        auto vert = (*allocConnections)[vec];
        //if(vert.entry->freshness){
        newCycle->cycle.push_back(vert.entry);
        if(vert.entry->freshness < oldVal){
          oldest = vert.entry;
          oldVal = oldest->freshness;
        }
        continue;
        //}
        //good = false;
        //break;
      }
      auto s = stateMap.find(stateFilter);
      if(s == stateMap.end()){
        return;
      }
      for(auto a : newCycle->cycle){
        if((s->second)->AllocsAllocatedInState.find(a) != (s->second)->AllocsAllocatedInState.end()){
          good = true;
          newCycle->states.insert(s->second);
        }
      }

      if(good){
        newCycle->weakPoint = oldest;
        this->cycles.push_back(newCycle);
      }
      else{
        delete newCycle;
      }
    }
  }

}
//This uses the connectionGraph and will print out any cycles it finds.
void AllocationTable::FindCyclesInConnectionGraph(){

  std::cerr << "Clearing previous cycles\n";
  //First clear out the previous cycles
  for(auto cycle : this->cycles){
    delete cycle;
  }
  this->cycles.clear();
  // typedef graph_traits<adjacency_list<vecS, vecS, directedS> >::vertex_descriptor vertex_t
  std::vector<int> component(num_vertices(*allocConnections)), discover_time(num_vertices(*allocConnections));
  std::vector<vertex_t> root(num_vertices(*allocConnections));
  std::vector<boost::default_color_type> color(num_vertices(*allocConnections)); 
  int num = strong_components(*allocConnections, make_iterator_property_map(component.begin(), boost::get(boost::vertex_index, *allocConnections)),
      root_map(make_iterator_property_map(root.begin(), boost::get(boost::vertex_index, *allocConnections))).
      color_map(make_iterator_property_map(color.begin(), get(boost::vertex_index, *allocConnections))).
      discover_time_map(make_iterator_property_map(discover_time.begin(), get(boost::vertex_index, *allocConnections))));


  std::unordered_map<int, std::vector<int64_t>*> sccToVertexMap;
  std::vector<int>::size_type i;
  for (i = 0; i != component.size(); ++i){
    DEBUG("Vertex %ld is in SCC %d\n", i, component[i]);
    auto verts = sccToVertexMap.find(component[i]);
    std::vector<int64_t>* SCCVec = nullptr;
    if(verts == sccToVertexMap.end()){
      SCCVec = new std::vector<int64_t>();
      sccToVertexMap.insert({component[i], SCCVec});
    }
    else{
      SCCVec = verts->second;
    }
    SCCVec->push_back(i);
  }

  //Now we have a map of all SCCs and the vertices in them
  //Now lets verify the freshness
  for(auto scc : sccToVertexMap){
    //Look for SCCs larger than 1
    if(scc.second->size() > 1){
      std::cerr << "New Cycle Found!\n";
      SmartPointerCycle* newCycle = new SmartPointerCycle();
      //Now we need to verify that the entries are all fresh...
      bool good = false;
      Allocation* oldest = nullptr;
      uint64_t oldVal = -1;
      for(auto vec : *(scc.second)){
        auto vert = (*allocConnections)[vec];
        //if(vert.entry->freshness){
        newCycle->cycle.push_back(vert.entry);
        if(vert.entry->freshness < oldVal){
          oldest = vert.entry;
          oldVal = oldest->freshness;
        }
        continue;
        //}
        //good = false;
        //break;
      }

      for(auto s : stateMap){
        for(auto a : newCycle->cycle){
          if((s.second)->AllocsAllocatedInState.find(a) != (s.second)->AllocsAllocatedInState.end()){
            good = true;
            newCycle->states.insert(s.second);
          }
        }
      }

      if(good){
        newCycle->weakPoint = oldest;
        this->cycles.push_back(newCycle);
      }
      else{
        delete newCycle;
      }
    }
  }

}
