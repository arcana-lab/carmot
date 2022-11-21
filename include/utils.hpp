#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <iterator>
#include <vector>
#include <unordered_set>
#include "./runtime/texas.hpp"
#include <fstream>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/unordered_set.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>



#define TRACE_SIZE 100

typedef struct Trace{
  long long buffer[TRACE_SIZE];
  int nptrs; 
  uint64_t accessFrequency = 0; 
  size_t hashPart1;

#ifdef MEMORYTOOL_STATISTICS
  uint64_t collisions = 0; // Useful only for statistics
#endif

  Trace(){
    //memset(buffer, 0, TRACE_SIZE*sizeof(long long));
    //nptrs = 0;
    //hashPart1 = 0;
  }

  Trace(long long *buffer, int nptrs, size_t hashPart1, uint64_t accessFrequency){
    memcpy(this->buffer, buffer, TRACE_SIZE*sizeof(long long));
    this->nptrs = nptrs;
    this->hashPart1 = hashPart1;
    this->accessFrequency = accessFrequency;
  }

  Trace(Trace &trace){
    memcpy(this->buffer, trace.buffer, TRACE_SIZE*sizeof(long long));
    this->nptrs = trace.nptrs;
    this->hashPart1 = trace.hashPart1;
    this->accessFrequency = trace.accessFrequency;
  }

  template<class Archive>
  void serialize(Archive & ar, const unsigned int version){
    ar & buffer;
    ar & nptrs;
    return;
  }
 
} Trace;

typedef struct DynamicTrace{
  Trace *trace = nullptr;
  uint64_t staticID = 0;
  uint64_t uniqueID = 0;
} DynamicTrace;

class UseToSave{
  public:
    Trace *trace;
    uint64_t staticID;
    uint64_t uniqueID;

    UseToSave():trace{nullptr},staticID{0},uniqueID{0}{}

    UseToSave(Trace *trace, uint64_t staticID, uint64_t uniqueID):trace{trace},staticID{staticID},uniqueID{uniqueID}{}

    /*
    UseToSave(UseToSave &use){
      this->trace = use.trace;
      this->staticID = use.staticID;
      this->uniqueID = use.uniqueID;
    }
    */
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version){
      ar & this->trace;
      ar & this->staticID;
      ar & this->uniqueID;
      return;
    }
 
};

class AllocationToSave{
  public:
    Trace *trace;
    uint64_t staticID;
    uint64_t uniqueID;
    uint64_t pointer;
    uint64_t length;
    std::unordered_set<uint64_t> offsets;
    std::vector<UseToSave> uses;
    bool isDirectState; // true only if the allocation was set using: SetDirectState()

    AllocationToSave():trace{nullptr},staticID{0},uniqueID{0},pointer{0},length{0},isDirectState{false}{}

    AllocationToSave(Trace *trace, stateInfoSetType *offsets, uint64_t staticID, uint64_t uniqueID, uint64_t pointer, uint64_t length, bool isDirectState):trace{trace},staticID{staticID},uniqueID{uniqueID},pointer{pointer},length{length},isDirectState{isDirectState}{
      if (offsets != nullptr){
        this->offsets.insert(offsets->begin(), offsets->end());
      }
    }

    /*
    AllocationToSave(AllocationToSave &allocation){
      this->trace = allocation.trace;
      this->staticID = allocation.staticID;
      this->uniqueID = allocation.uniqueID;
      this->pointer = allocation.pointer;
      this->length = allocation.length;
      this->offsets = allocation.offsets;
      for (auto &use: allocation.uses){
        this->uses.push_back(use);
      }
    }
    */

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version){
      ar & this->trace;
      ar & this->staticID;
      ar & this->uniqueID;
      ar & this->pointer;
      ar & this->length;
      ar & this->offsets;
      ar & this->uses;
      ar & this->isDirectState;
      return;
    }
 
};

class StateSetToSave{
  public:
    std::vector<AllocationToSave> allocations;

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version){
      ar & this->allocations;
      return;
    }
 
};

class CycleToSave{
  public:
    std::vector<AllocationToSave> cycle;
    AllocationToSave weakPoint;

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version){
      ar & this->cycle;
      ar & this->weakPoint;
      return;
    }
};

class StateToSave{
  public:
    std::string functionName;
    uint64_t lineNumber;

    StateSetToSave I;
    StateSetToSave O;
    StateSetToSave IO;
    StateSetToSave CO;
    StateSetToSave TO;
    StateSetToSave CIO;
    StateSetToSave TIO;

    std::vector<CycleToSave> cycles;

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version){
      ar & this->functionName;
      ar & this->lineNumber;
      ar & this->I;
      ar & this->O;
      ar & this->IO;
      ar & this->CO;
      ar & this->TO;
      ar & this->CIO;
      ar & this->TIO;
      ar & cycles;
      return;
    }

};

class StatesToSave{
  public:
    std::unordered_map<uint64_t, StateToSave> states;

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version){
      ar & this->states;
      return;
    }
 
};


bool traceCompare(Trace &trace1, Trace &trace2);
