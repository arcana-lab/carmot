#include "./include/CheckForInductionVariables.hpp"

#include "srcCloneFunc/include/TagROIs.hpp"
#include "srcCloneFunc/include/MetadataInstruction.hpp"

bool checkForInductionVariable(Noelle &noelle, Instruction *roiInst, StayConnectedNestedLoopForestNode *L){
  LoopStructure *LS = L->getLoop();
  LoopDependenceInfo *LDI = noelle.getLoop(L);

  InductionVariableManager *loopInductionVariableManager = LDI->getInductionVariableManager();
  InductionVariable *loopGovIV = loopInductionVariableManager->getLoopGoverningInductionVariable(*LS);
  // There is no IV
  // Assumption: the AllocaInst promoted in an earlier pass was loop governing
  if (loopGovIV == nullptr){
    errs() << "DEBUG: ROI " << *roiInst << " has no loopGovIV.\n";
    return false;
  }

  auto loopGovIVAttribution = LDI->getLoopGoverningIVAttribution();
  Value *exitConditionValue = loopGovIVAttribution->getExitConditionValue();
  // Cannot compute exit value of loop governing IV
  if (exitConditionValue == nullptr){
    errs() << "DEBUG: ROI " << *roiInst << " has no exitConditionValue.\n";
    return false;
  }
  errs() << "DEBUG: ROI " << *roiInst << " has exitConditionValue " << *exitConditionValue << "\n";

  // Cannot compute start value of loop governing IV
  Value *startValue = loopGovIV->getStartValue();
  if (startValue == nullptr){
    errs() << "DEBUG: ROI " << *roiInst << " has no startValue.\n";
    return false;
  }
  errs() << "DEBUG: ROI " << *roiInst << " has startValue " << *startValue << "\n";

  // Cannot compute single step value of loop governing IV
  Value *stepValue = loopGovIV->getSingleComputedStepValue();
  if (stepValue == nullptr){
    errs() << "DEBUG: ROI " << *roiInst << " has no single stepValue.\n";
    return false;
  }
  errs() << "DEBUG: ROI " << *roiInst << " has single stepValue " << *stepValue << "\n";


  errs() << "DEBUG: ROI " << *roiInst << " has loopGovIV";
  for (auto inst : loopGovIV->getAllInstructions()){
    errs() << " " << *inst;
  }
  errs() << "\n";

  errs() << "DEBUG: ROI " << *roiInst << " has loopGovIV with " << loopGovIV->getComputationOfStepValue().size() << " step values";
  for (auto inst : loopGovIV->getComputationOfStepValue()){
    errs() << " " << *inst;
  }
  errs() << "\n";

  return true;
}

std::vector<Instruction*> checkForInductionVariables(Noelle &noelle, std::unordered_map<Instruction*, StayConnectedNestedLoopForestNode*> &roiToLoopMap){
  std::vector<Instruction*> roisToPromoteAllocas;
  for (auto elem : roiToLoopMap){
    Instruction* roiStartInst = elem.first;
    errs() << "DEBUG: ROI " << *roiStartInst << "\n";
    StayConnectedNestedLoopForestNode *L = elem.second;
    if (!checkForInductionVariable(noelle, roiStartInst, L)){
      continue;
    }

    roisToPromoteAllocas.push_back(roiStartInst);
  }
   
  return roisToPromoteAllocas;
}

std::unordered_map<uint64_t, Instruction*> getIdToRoiMap(std::vector<Instruction*> &rois){
  std::unordered_map<uint64_t, Instruction*> idToRoiMap;
  for (auto roi : rois){
    std::string idStr = getMetadata(roi, ROI_ID);
    uint64_t id = (uint64_t) std::stoi(idStr);
    idToRoiMap[id] = roi;
  }

  return idToRoiMap;
}

std::vector<Instruction*> getOriginalROIsToPromoteAllocas(Module &M, std::vector<Instruction*> &roisCloned){
  ROIInstVisitor roiInstVisitor(M, MEMORYTOOL_START_TRACKING);
  roiInstVisitor.visit(M);
  std::vector<Instruction*> &roisOriginal = roiInstVisitor.roiStartCallBases;
  std::unordered_map<uint64_t, Instruction*> idToRoiOriginalMap = getIdToRoiMap(roisOriginal);
  std::unordered_map<uint64_t, Instruction*> idToRoiClonedMap = getIdToRoiMap(roisCloned);

  std::vector<Instruction*> roisOriginalToPromoteAllocas;
  for (auto elem : idToRoiClonedMap){
    uint64_t id = elem.first;
    if (idToRoiOriginalMap.count(id) == 0){
      errs() << "ERROR: corresponding original ROI with ID " << id << " not found. Abort.\n";
      abort();
    }
    Instruction *roiOriginal = idToRoiOriginalMap[id];
    roisOriginalToPromoteAllocas.push_back(roiOriginal);
  }

  return roisOriginalToPromoteAllocas;
}

