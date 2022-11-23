#include "include/ROIManager.hpp"

CallInstVisitor::CallInstVisitor(Function *functionOfInterest):functionOfInterest{functionOfInterest}{ }

void CallInstVisitor::visitCallBase(CallBase &inst) {
  // Create a map: function where the call is -> set of calls to function of interest
  Function *calledFunc = inst.getCalledFunction();
  if (calledFunc == this->functionOfInterest){
    Function *funcCallInstIsIn = inst.getFunction();
    this->functionToCallsOfInterest[funcCallInstIsIn].insert(&inst);
  }

  return;
}

void ROIManager::print(void){
  for (auto elem : this->funcToRoisMap){
    for (auto roi : elem.second){
      roi->print();
    }
  }

  return;
}

std::vector<ROI*> ROIManager::visitROIs(std::map<Function*, std::set<CallBase*>> mapToStart){
  std::vector<ROI*> rois;
  // Iterate over every function that has a call to start tracking
  for (auto elem : mapToStart){
    // Get key
    Function *func = elem.first;

    // Iterate over calls to start tracking for the current function of interest
    std::set<CallBase*> &callInsts = elem.second;
    for (auto callToStart : callInsts){
      std::vector<CallBase*> callsToStop;

      // Get staticID
      bool oneUser = callToStart->hasOneUse();
      if (!oneUser){
        for (Value *userOfStateID : callToStart->users()){
          CallBase *callInst = dyn_cast<CallBase>(userOfStateID);
          if (callInst != nullptr){
            if (callInst->getCalledFunction() == this->stopFunc){
              callsToStop.push_back(callInst);
            }
          }
        }

        if (callsToStop.size() == 0){
          errs() << "ERROR: callsToStop.size() == 0. Cannot find ROI. Abort.\n";
          abort();
        }

        ROI *roi = new ROI(callToStart, callsToStop);
        rois.push_back(roi);

        continue;

        //errs() << "ERROR: call to start tracking (i.e., stateID) has more than 1 use. It has " << callToStart->getNumUses() << " uses. Cannot find ROI. Abort.\n";
        //abort();
      }

      User *user = callToStart->user_back();
      StoreInst *storeInst = dyn_cast<StoreInst>(user);
      if (storeInst == nullptr){
        errs() << "ERROR: user " << *user << " is not a store instruction. Does not follow pattern to find ROI. Abort.\n";
        abort();
      }

      Value *allocaOfStateID = storeInst->getPointerOperand();
      for (Value *userOfStateID : allocaOfStateID->users()){
        // If user of stateID is not a load instruction, then keep going
        LoadInst *loadInst = dyn_cast<LoadInst>(userOfStateID);
        if (loadInst == nullptr){
          continue;
        }

        // If it is a load instruction, then check if it has only one use, and if that use is in a stop tracking call instruction
        bool oneUser = loadInst->hasOneUse();
        if (!oneUser){
          errs() << "ERROR: load of stateID has more than 1 use. It has " << loadInst->getNumUses() << " uses. Cannot find ROI. Abort.\n";
          abort();
        }

        User *user = loadInst->user_back();
        CallBase *callInst = dyn_cast<CallBase>(user);
        if (callInst == nullptr){
          errs() << "ERROR: user " << *user << " is not a CallBase instruction. Does not follow pattern to find ROI. Abort.\n";
          abort();
        }
        // If stateID is used in end context function, then just ignore it and continue.
        if (callInst->getCalledFunction() == contextFunc){
          continue;
        }
        if (callInst->getCalledFunction() != this->stopFunc){
          errs() << "ERROR: call instruction " << *callInst << " is not calling stop tracking. Does not follow pattern to find ROI. Abort.\n";
          abort();
        }

        callsToStop.push_back(callInst);
      } // iterating over calls to stop tracking

      if (callsToStop.empty()){
        errs() << "ERROR: no calls to stop tracking found. Abort.\n";
        abort();
      }

      ROI *roi = new ROI(callToStart, callsToStop);
      rois.push_back(roi);
    } // iterating over calls to start tracking of a specific function
  } // iterating over all functions that have at least one call to start tracking

  return rois;
}

std::vector<ROI*> ROIManager::getROIs(Module &M){
  std::vector<ROI*> rois;
  for (auto elem : this->funcToRoisMap){
    std::vector<ROI*> &funcRois = elem.second;
    for (auto roi : funcRois){
      rois.push_back(roi);
    }
  }

  return rois;
}

std::vector<ROI*> ROIManager::computeROIs(Function &F){
  // Find calls to start tracking
  CallInstVisitor callToStartInstVisitor(this->startFunc);
  callToStartInstVisitor.visit(F);

  std::vector<ROI*> rois = visitROIs(callToStartInstVisitor.functionToCallsOfInterest);

  return rois;
}

std::vector<ROI*> ROIManager::getROIs(Function &F){
  std::vector<ROI*> rois;

  auto found = this->funcToRoisMap.find(&F);
  if (found != this->funcToRoisMap.end()){
    rois = found->second;
  }

  return rois;
}

ROIManager::ROIManager(Module &M) {
  this->startFunc = M.getFunction(MEMORYTOOL_START_TRACKING);
  this->stopFunc = M.getFunction(MEMORYTOOL_STOP_TRACKING);
  this->contextFunc = M.getFunction(MEMORYTOOL_CONTEXT);

  for (auto &F : M){
    this->funcToRoisMap[&F] = this->computeROIs(F);
  }

}

std::unordered_set<Instruction*> ROIManager::computeROIsLoadsStores(void){
  std::unordered_set<Instruction*> roisLoadsStores;
  for (auto elem : this->funcToRoisMap){
    for (auto roi : elem.second){
      CallBase *start = roi->start;
      // We already know start has one user and it's a StoreInst
      User *user = start->user_back();
      StoreInst *storeInst = cast<StoreInst>(user);
      roisLoadsStores.insert(storeInst);

      Value *allocaOfStateID = storeInst->getPointerOperand();
      errs() << "ALOCA " << *allocaOfStateID << "\n";
      for (Value *userOfStateID : allocaOfStateID->users()){
        errs() << "USERB " << *userOfStateID << "\n";
        LoadInst *loadUserOfStateID = dyn_cast<LoadInst>(userOfStateID);
        StoreInst *storeUserOfStateID = dyn_cast<StoreInst>(userOfStateID);
        if ((loadUserOfStateID != nullptr) || (storeUserOfStateID != nullptr)){
          Instruction *instUserOfStateID = cast<Instruction>(userOfStateID);
          roisLoadsStores.insert(instUserOfStateID);
          errs() << "USER " << *userOfStateID << "\n";
        }
      }
    }
  }

  return roisLoadsStores;
}

std::unordered_set<Instruction*> ROIManager::getROIsLoadsStores(void){
  this->roisLoadsStores = this->computeROIsLoadsStores();
  return this->roisLoadsStores;
}
