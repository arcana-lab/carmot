#include "./include/PromoteAllocaToReg.hpp"
#include "./include/Utils.hpp"
//#include "./srcCARMOT/include/ROIManager.hpp"
#include "./srcCloneFunc/include/MetadataInstruction.hpp"
#include "./srcCloneFunc/include/TagROIs.hpp"

bool promoteSingleAlloca(AllocaInst *AI, DominatorTree &DT, AssumptionCache &AC){
  std::vector<AllocaInst *> Allocas;
  if (isAllocaPromotable(AI)){
    Allocas.push_back(AI);
  } else {
    errs() << "ERROR: cannot promote alloca " << *AI << " . Abort.\n";
    abort();
  }

  PromoteMemToReg(Allocas, DT, &AC);

  // If we get here we modified the IR
  return true;
}

std::unordered_map<Function*, std::unordered_map<Instruction*, StayConnectedNestedLoopForestNode*>> getFunctionToLoopsMap(Module &M, Noelle &noelle){
  std::unordered_map<Function*, std::unordered_map<Instruction*, StayConnectedNestedLoopForestNode*>> functionToLoopsMap;
  ROIInstVisitor roiInstVisitor(M, MEMORYTOOL_START_TRACKING);
  roiInstVisitor.visit(M);
  //ROIManager roiManager(M);
  //std::vector<ROI*> rois = roiManager.getROIs(M);
  std::vector<Instruction*> &roisOrig = roiInstVisitor.roiStartCallBases;
  std::vector<Instruction*> &roisCloned = roiInstVisitor.roiStartCallBasesCloned;
  std::vector<Instruction*> rois(roisOrig.begin(), roisOrig.end());
  rois.insert(rois.end(), roisCloned.begin(), roisCloned.end());
  for (Instruction *roi : rois){
    CallBase *roiStart = cast<CallBase>(roi);
    MDNode *n = roiStart->getMetadata(PROMOTE_ALLOCAS);
    if (n == nullptr){
      continue;
    }

    Function *func = roiStart->getFunction();
    StayConnectedNestedLoopForestNode *L = getROILoop(noelle, roiStart);
    // ROI is not in a loop, then continue
    if (L == nullptr){
      continue;
    }

    functionToLoopsMap[func][roiStart] = L;
  }

  return functionToLoopsMap;
}

bool promoteAllocasROIs(std::unordered_map<Instruction*, StayConnectedNestedLoopForestNode*> &roiToLoopMap, DominatorTree &DT, AssumptionCache &AC){
  bool modified = false;
  for (auto elem : roiToLoopMap){
    Instruction *roiStartInst = elem.first;
    StayConnectedNestedLoopForestNode *L = elem.second;
    LoopStructure *LS = L->getLoop();
    std::set<AllocaInst*> roiAllocaInsts; // We need a set because different loops might use the same alloca
    BasicBlock *headerBB = LS->getHeader();
    // Get AllocaInst to be promoted by looking at loop header
    for (auto &headerI : *headerBB){
      unsigned numOfOperands = headerI.getNumOperands();
      for (unsigned i = 0; i < numOfOperands; ++i){
        Value *operand = headerI.getOperand(i);
        AllocaInst *allocaInst = dyn_cast<AllocaInst>(operand);

        if (allocaInst == nullptr){
          continue;
        }  

        if (!allocaInst->getAllocatedType()->isIntegerTy()){
          continue; // Induction variables can only be integers
        }

        roiAllocaInsts.insert(allocaInst);
      }
    }

    if (roiAllocaInsts.size() != 1){
      errs() << "WARNING: more than one alloca inst candidate. Continue\n";
      continue;
    }

    for (auto allocaInst : roiAllocaInsts){
      modified |= promoteSingleAlloca(allocaInst, DT, AC);
    }

    //setMetadata(roiStartInst, NUM_PROMOTED_ALLOCAS, std::to_string(roiAllocaInsts.size()));
  }

  return modified;
}

