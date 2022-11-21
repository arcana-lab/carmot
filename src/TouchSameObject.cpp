#include "./include/TouchSameObject.hpp"
#include <algorithm>

struct TouchObjectInstVisitor : public InstVisitor<TouchObjectInstVisitor> {
  Noelle &noelle;
  bool enableDFAObjGranularity;
  std::unordered_map<Instruction*, Value*> instToObjMapLoad;
  std::unordered_map<Instruction*, Value*> instToObjMapStore;
  std::unordered_set<Function*> &functionsToRunDFA;
  std::unordered_map<Function*, std::vector<ROI*>> funcToRoiMap;
  std::unordered_map<Function*, DominatorSummary*> dominatorSummaries;

  TouchObjectInstVisitor(Module &M, Noelle &noelle, bool enableDFAObjGranularity, std::unordered_set<Function*> &functionsToRunDFA, std::vector<ROI*> &rois):noelle{noelle},enableDFAObjGranularity{enableDFAObjGranularity},functionsToRunDFA{functionsToRunDFA}{
    for (auto &elem : rois){
      Function *startTrackingCaller = elem->start->getFunction();
      this->funcToRoiMap[startTrackingCaller].push_back(elem);
    }

    // Precompute DominatorSummary from noelle only one time
    for (auto &elem : this->funcToRoiMap){
      Function *func = elem.first;
      DominatorSummary *dominatorSummary = noelle.getDominators(func);
      this->dominatorSummaries[func] = dominatorSummary;
    }
  }

  void visitInstObjGranularity(Instruction &inst){
    Value *addr = nullptr;
    if (LoadInst *loadInst = dyn_cast<LoadInst>(&inst)){
      addr = loadInst->getPointerOperand();
      GetElementPtrInst *gepInst = dyn_cast<GetElementPtrInst>(addr);
      if (gepInst == nullptr){
        return;
      }

      Value *obj = gepInst->getPointerOperand();
      this->instToObjMapLoad[&inst] = obj;

    } else if (StoreInst *storeInst = dyn_cast<StoreInst>(&inst)){
      addr = storeInst->getPointerOperand();
      GetElementPtrInst *gepInst = dyn_cast<GetElementPtrInst>(addr);
      if (gepInst == nullptr){
        return;
      }

      Value *obj = gepInst->getPointerOperand();
      this->instToObjMapStore[&inst] = obj;

    } else {
      errs() << "ERROR: Object granularity. Touch same object, not a load, not a store. Abort.";
      abort();
    }

    return;
  }

  void visitInstPtrGranularity(Instruction &inst){
    Value *addr = nullptr;
    if (LoadInst *loadInst = dyn_cast<LoadInst>(&inst)){
      addr = loadInst->getPointerOperand();
      this->instToObjMapLoad[&inst] = addr;
    } else if (StoreInst *storeInst = dyn_cast<StoreInst>(&inst)){
      addr = storeInst->getPointerOperand();
      this->instToObjMapStore[&inst] = addr;
    } else {
      errs() << "ERROR: Instruction pointer granularity. Touch same object, not a load, not a store. Abort.";
      abort();
    }

    return;
  }

  template <typename I> void visitLoadStoreInst(I &inst) {
    Function *functionInstIsIn = inst.getFunction();
    if (this->functionsToRunDFA.count(functionInstIsIn) == 0){
      return;
    }

    // Special case: inst is in the function where ROI is
    if (this->funcToRoiMap.count(functionInstIsIn)){
      DominatorSummary *dominatorSummary = this->dominatorSummaries[functionInstIsIn];
      DomTreeSummary &DT = dominatorSummary->DT;
      DomTreeSummary &PDT = dominatorSummary->PDT;
      bool considerThisInst = true;
      for (auto &roi : this->funcToRoiMap[functionInstIsIn]){
        bool startDominateInst = false;
        if (DT.dominates(roi->start, &inst)){
          startDominateInst = true;
        }

        bool stopPostDominatesInst = false;
        for (auto stop : roi->stops){
          if (PDT.dominates(stop, &inst)){
            stopPostDominatesInst = true;
            break;
          }
        }

        // All ROIs the instruction is in must satisfy this condition
        considerThisInst &= (startDominateInst && stopPostDominatesInst);
      }

      if (!considerThisInst){
        return;
      }

    }

    if (this->enableDFAObjGranularity){
      visitInstObjGranularity(inst);
    } else {
      visitInstPtrGranularity(inst);
    }

    return;
  }

  void visitStoreInst(StoreInst &inst){
    visitLoadStoreInst(inst);

    return;
  }

  void visitLoadInst(LoadInst &inst){
    visitLoadStoreInst(inst);

    return;
  }

};

std::unordered_map<Function*, DataFlowResult*> getFuncToDfrMap(Noelle &noelle, unordered_map<Instruction*, Value*> &instToObjMap, std::unordered_set<Function*> &functionsToRunDFA){
  // Data flow result map for each function
  std::unordered_map<Function*, DataFlowResult*> funcToDfr;

  /*
   * Fetch the data flow engine.
   */
  auto dfe = noelle.getDataFlowEngine();

  /*
   * Define the data flow equations
   */
  auto computeGEN = [instToObjMap](Instruction *i, DataFlowResult *df) { // Gen[i] is the object the store inst is storing things into
    if (instToObjMap.count(i) == 0){
      return;
    }

    Value *obj = instToObjMap.at(i);
    auto& gen = df->GEN(i);
    gen.insert(obj);

    return ;
  };

  auto computeKILL = [](Instruction *, DataFlowResult *) { // Empty
    return ;
  };

  auto computeOUT = [](Instruction *i, std::set<Value *>& OUT, DataFlowResult *df) { // Union of IN and GEN of the current inst
    auto& genI = df->GEN(i);
    auto& inI = df->IN(i);

    OUT.insert(genI.begin(), genI.end());
    OUT.insert(inI.begin(), inI.end());

    return ;
  };

  auto computeIN = [](Instruction *i, std::set<Value *>& IN, Instruction *pred, DataFlowResult *df) { // Intersection of OUT of all predecessors
    auto &outP = df->OUT(pred);
    std::set<Value*> currIN = IN;
    IN.clear();
    std::set_intersection(outP.begin(), outP.end(), currIN.begin(), currIN.end(), std::inserter(IN, IN.end()));

    return ;
  };

  auto initializeIN = [instToObjMap](Instruction *i, std::set<Value*> &IN) {
    Function *instF = i->getFunction();
    BasicBlock &entryBB = instF->getEntryBlock();
    Instruction *firstInst = &(entryBB.front());

    if (firstInst == i){
      return;
    }

    for (auto &elem : instToObjMap){
      IN.insert(elem.second);
    }

    return;
  };

  auto initializeOUT = [](Instruction *i, std::set<Value*> &OUT) {
    return;
  };

  // Run DFA
  for (auto F : functionsToRunDFA){
    auto customDfr = dfe.applyForward(F, computeGEN, computeKILL, initializeIN, initializeOUT, computeIN, computeOUT);
    funcToDfr[F] = customDfr;
  }

  return funcToDfr;
}

std::unordered_set<Instruction*> getInstsToSkip(std::unordered_map<Function*, DataFlowResult*> &funcToDfr, std::unordered_map<Instruction*, Value*> &instToObjMap, std::unordered_set<Function*> &functionsToRunDFA){
  std::unordered_set<Instruction*> instsToSkip;
  for (auto F : functionsToRunDFA){
    DataFlowResult *dfr = funcToDfr[F];
    for (auto &BB : *F){
      for (auto &I : BB){
        if (instToObjMap.count(&I) == 0){
          continue;
        }

        Value *obj = instToObjMap[&I];
        std::set<Value*> &IN = dfr->IN(&I);

        if (IN.count(obj)){
          instsToSkip.insert(&I);
        }

      }
    }
  }

  return instsToSkip;
}

std::unordered_set<Instruction*> getTouchSameObjectInsts(Module &M, Noelle &noelle, std::unordered_set<Function*> &functionsToRunDFA, std::vector<ROI*> &rois, bool enableDFAObjGranularity){
  // Create inst to obj map
  TouchObjectInstVisitor IV(M, noelle, enableDFAObjGranularity, functionsToRunDFA, rois);
  IV.visit(M);

  // Run DFA for loads
  std::unordered_map<Instruction*, Value*> &instToObjMapLoad = IV.instToObjMapLoad;
  std::unordered_map<Function*, DataFlowResult*> funcToDfrLoad = getFuncToDfrMap(noelle, instToObjMapLoad, functionsToRunDFA);
  std::unordered_set<Instruction*> instsToSkipLoad = getInstsToSkip(funcToDfrLoad, instToObjMapLoad, functionsToRunDFA);

  // Run DFA for stores
  std::unordered_map<Instruction*, Value*> &instToObjMapStore = IV.instToObjMapStore;
  std::unordered_map<Function*, DataFlowResult*> funcToDfrStore = getFuncToDfrMap(noelle, instToObjMapStore, functionsToRunDFA);
  std::unordered_set<Instruction*> instsToSkipStore = getInstsToSkip(funcToDfrStore, instToObjMapStore, functionsToRunDFA);

  // Union the two previous results
  std::unordered_set<Instruction*> instsToSkip;
  std::set_union(instsToSkipLoad.begin(), instsToSkipLoad.end(), instsToSkipStore.begin(), instsToSkipStore.end(), std::inserter(instsToSkip, instsToSkip.end()));

  return instsToSkip;
}

