#include "include/TagROIs.hpp"
#include "include/MetadataInstruction.hpp"

#include <cstdint>
#include <vector>

bool tagROIWithID(Instruction *roiStartInst, uint64_t roiID){
  return setMetadata(roiStartInst, ROI_ID, std::to_string(roiID));
}

bool tagROIsWithIDs(Module &M, std::string roiStartFuncName){
  bool modified = false;

  ROIInstVisitor roiInstVisitor(M, roiStartFuncName);
  roiInstVisitor.visit(M);

  std::vector<Instruction*> &roiStartCallBases = roiInstVisitor.roiStartCallBases;

  uint64_t roiID = 0;
  for (auto roiStartCallBase : roiStartCallBases){
    modified |= tagROIWithID(roiStartCallBase, roiID);
    roiID += 1;
  }

  return modified;
}

bool tagROIToPromoteAllocas(Instruction *roiStartInst, std::string promoteAllocas){
  return setMetadata(roiStartInst, PROMOTE_ALLOCAS, promoteAllocas);
}

bool tagROIsToPromoteAllocas(Module &M, std::string roiStartFuncName){
  bool modified = false;

  ROIInstVisitor roiInstVisitor(M, roiStartFuncName);
  roiInstVisitor.visit(M);

  std::vector<Instruction*> &roiStartCallBases = roiInstVisitor.roiStartCallBasesCloned;

  for (auto roiStartCallBase : roiStartCallBases){
    modified |= tagROIToPromoteAllocas(roiStartCallBase, "true");
  }

  return modified;
}
