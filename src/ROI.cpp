#include "include/ROI.hpp"

#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"

ROI::ROI(CallBase *start, std::vector<CallBase*> &stops):start{start},stops{stops} { }

DILocation* ROI::getInstDebugLoc(Instruction *inst){
  // Get ROI debug info
  DILocation *debugLoc = inst->getDebugLoc();
  if (debugLoc == nullptr){
    errs() << "WARNING: call instruction in ROI: " << inst << " does not have debug information\n";
    abort();
  }

  return debugLoc;
}

void ROI::print(void){
  DILocation *debugLocStart = this->getInstDebugLoc(this->start);
  errs() << "Call to start:\t" << *(this->start) << ":" << debugLocStart->getLine();

  for (auto stop : this->stops){
    DILocation *debugLocStop = this->getInstDebugLoc(stop);
    errs() << "Call to stop:\t" << *stop << ":" << debugLocStop->getLine() << "\n";
  }

  return;
}

/* EXAMPLE
func(){

  if (){
    start()
    ROI0
    stop()
  }

  start()
  start()
  a += 1
  stop() // check: does stop() post-dominates the closest start (i.e.-> they are control-equivalent)
  stop()

  start()
  ROI2
  stop()

}
*/
