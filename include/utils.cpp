#include "utils.hpp"

bool traceCompare(Trace &trace1, Trace &trace2){
  if (trace1.nptrs != trace2.nptrs){
    return false;
  }

  int res = memcmp(trace1.buffer , trace2.buffer, trace1.nptrs);
  if(res != 0){
    return false;
  }

  return true;
}

