#include "noelle/core/Noelle.hpp"
#include "./include/Utils.hpp"
#include "./include/ROI.hpp"
#include "./include/ROIManager.hpp"
#include "./include/DebugData.hpp"
#include "./include/CatPass.hpp"
#include "./include/AvoidLocalVariableInstrumentation.hpp"
#include "./include/ClusterCallstackInstrumentation.hpp"
#include "./include/InstrumentLoopInvariantStoreAddressOnce.hpp"
#include "./include/MostlyInvariant.hpp"
#include "./include/Invariant.hpp"
#include "./include/BaseAddressMostlyInvariant.hpp"
#include "./include/Induction.hpp"
#include "./include/TouchSameObject.hpp"
#include "./include/OnlyRead.hpp"
#include "./include/DirectState.hpp"

#include "llvm/Transforms/Utils/ModuleUtils.h"

#include <cstdint>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <fstream>

using namespace llvm;

static cl::opt<bool> disableStateInBytesOpt("disable-state-in-bytes", cl::ZeroOrMore, cl::Hidden, cl::desc("Disable measuring state in bytes"));
static cl::opt<bool> disableInputOpt("disable-state-input", cl::ZeroOrMore, cl::Hidden, cl::desc("Disable tracking of input state set"));
static cl::opt<bool> disableOutputOpt("disable-state-output", cl::ZeroOrMore, cl::Hidden, cl::desc("Disable tracking of output state set"));
static cl::opt<bool> disableCloneableOpt("disable-state-cloneable", cl::ZeroOrMore, cl::Hidden, cl::desc("Disable tracking of cloneable state set"));
static cl::opt<bool> disableTransferOpt("disable-state-transfer", cl::ZeroOrMore, cl::Hidden, cl::desc("Disable tracking of transfer state set"));
static cl::opt<bool> cycleFindingOpt("cycle-finding", cl::ZeroOrMore, cl::Hidden, cl::desc("If this option is used, then we remove all calls to free and delete"));
static cl::opt<bool> disableStateUsesOpt("disable-state-uses", cl::ZeroOrMore, cl::Hidden, cl::desc("Disable state uses"));
static cl::opt<bool> disableCallstackOpt("disable-callstack", cl::ZeroOrMore, cl::Hidden, cl::desc("Disable callstack"));
static cl::opt<bool> enableDFAObjGranularityOpt("enable-DFA-obj-granularity", cl::ZeroOrMore, cl::Hidden, cl::desc("Enable DFA object granularity"));
static cl::opt<bool> disableStateUsesInstrumentationOpt("disable-state-uses-instrumentation", cl::ZeroOrMore, cl::Hidden, cl::desc("Disable state uses instrumentation"));
static cl::opt<bool> disableAllInstrumentationOpt("disable-all-instrumentation", cl::ZeroOrMore, cl::Hidden, cl::desc("Disable all instrumentation"));


namespace {

  std::set<std::string> knowns = {MEMORYTOOL_START_TRACKING, MEMORYTOOL_STOP_TRACKING, MEMORYTOOL_PROLOGUE, MEMORYTOOL_EPILOGUE, MEMORYTOOL_CONTEXT};

  uint64_t findStructSize(Type *sType);
  uint64_t findArraySize(Type *aType);

  // Compute the size of an array variable
  uint64_t findArraySize(Type *aType) {
    Type *insideType;
    uint64_t size = aType->getArrayNumElements();
#if DEBUG == 1
    errs() << "Num elements in array: " << size << "\n";
#endif
    insideType = aType->getArrayElementType();
    if (insideType->isArrayTy()) {
      size = size * findArraySize(insideType);
    } else if (insideType->isStructTy()) {
      size = size * findStructSize(insideType);
    } else if (insideType->getPrimitiveSizeInBits() > 0) {
      size = size * (insideType->getPrimitiveSizeInBits() / 8);
    } else if (insideType->isPointerTy()) {
      size = size + 8;
    } else {
      errs() << "Error(Array): cannot determing size: " << *insideType << "\n";
      return 0;
    }
#if DEBUG == 1
    errs() << "Returning: " << size << "\n";
#endif
    return size;
  }

  // Compute the size of a struct variable
  uint64_t findStructSize(Type *sType) {
    uint64_t size = 0;
    for (int i = 0; i < sType->getStructNumElements(); i++) {
      if (sType->getStructElementType(i)->isArrayTy()) {
        size = size + findArraySize(sType->getStructElementType(i));
      } else if (sType->getStructElementType(i)->isStructTy()) {
        size = size + findStructSize(sType->getStructElementType(i));
      } else if (sType->getStructElementType(i)->getPrimitiveSizeInBits() > 0) {
        size =
          size + (sType->getStructElementType(i)->getPrimitiveSizeInBits() / 8);
      } else if (sType->getStructElementType(i)->isPointerTy()) {
        // This is bad practice to just assume 64-bit system... but whatever
        size = size + 8;
      } else {
        errs() << "Error(Struct): Cannot determine size:"
          << *(sType->getStructElementType(i)) << "\n";
        return 0;
      }
    }
#if DEBUG == 1
    errs() << "Returning: " << size << "\n";
#endif
    return size;
  }

  // Similar to python: stringToSearch = stringToSearchArg.split(delimiter)[-1]
  std::string splitAndGetLast(std::string stringToSearchArg,
      std::string delimiter) {
    // Copy string argument, it will be modified
    std::string stringToSearch(stringToSearchArg);

    size_t pos = 0;
    std::string token;
    while ((pos = stringToSearch.find(delimiter)) != std::string::npos) {
      token = stringToSearch.substr(0, pos);
      stringToSearch.erase(0, pos + delimiter.length());
    }

    return stringToSearch;
  }

  // Enum for functions that perform memory operations (allocations and free)
  enum AllocFunctions {
    _malloc = 0,
    _calloc,
    _realloc,
    _jemalloc,
    _mmap,
    _mremap,
    _new,
    _munmap,
    _delete,
    _free,
    _unknown
  };

  // Map from function names (mangled) to alloc functions
  std::unordered_map<std::string, AllocFunctions> functionCalls = {
    {"malloc", _malloc},     {"calloc", _calloc}, {"realloc", _realloc},
    {"jemalloc", _jemalloc}, {"mmap", _mmap},     {"mremap", _mremap},
    {"_Znwm", _new},         {"munmap", _munmap}, {"_ZdlPv", _delete},
    {"free", _free}};

  // Instruction visitor for malloc, new, calloc, realloc, load, store, alloca,
  // return, free, delete, etc. (no globals, there is no "globals visitor" as of
  // now)
  struct MyInstVisitor : public InstVisitor<MyInstVisitor> {
    std::vector<Instruction *> callsToStartTracking;
    std::vector<ReturnInst *> returns;
    std::vector<InvokeInst *> invokes;
    std::vector<Entry *> loads;
    std::vector<StoreInst *> escapes;
    std::vector<Instruction *> deallocs; // Used for free and delete
    std::vector<Entry *> stores;
    std::vector<Entry *> allocations; // Used for malloc, new, alloca
    std::vector<Entry *> callocs;
    std::vector<Entry *> reallocs;
    std::vector<Entry *> unknowns; // Used for library calls
    std::unordered_map<std::string, AllocFunctions> &functionCalls;
    std::unordered_map<Function *, std::vector<Instruction *>> functionAllocaMap;
    std::unordered_set<Function *> &functionsToInstrumentAllocaInst;
    std::unordered_set<Function *> &functionsToInstrumentLoadStore;
    std::unordered_set<Function *> functionsToSkipForLoadAndStore;
    std::unordered_map<Instruction*, Entry*> instToEntryMap;
    std::unordered_set<Instruction *> &touchesToSkip;
    Module &M;

    MyInstVisitor(
        std::unordered_set<Function *> &functionsThatDirectlyCallStartTracking,
        std::unordered_set<Function *> &functionsToInstrumentAllocaInst,
        std::unordered_map<std::string, AllocFunctions> &functionCalls,
        Module &M, std::unordered_set<Instruction *> &touchesToSkip)
      : functionsToInstrumentLoadStore{functionsThatDirectlyCallStartTracking},
      functionsToInstrumentAllocaInst{functionsToInstrumentAllocaInst},
      functionCalls{functionCalls}, M{M}, touchesToSkip{touchesToSkip} {}

    ~MyInstVisitor() {
      for (auto elem : this->allocations) {
        delete elem;
      }
      this->allocations.clear();

      for (auto elem : this->callocs) {
        delete elem;
      }
      this->callocs.clear();

      for (auto elem : this->reallocs) {
        delete elem;
      }
      this->reallocs.clear();

      for (auto elem : this->stores) {
        delete elem;
      }
      this->stores.clear();

      for (auto elem : this->unknowns) {
        delete elem;
      }
      this->unknowns.clear();
    }

    // Collect calls to malloc, realloc, etc.
    template <typename I> void visitCallInvokeInst(I &inst) {
      // Get the called function
      Function *fp = inst.getCalledFunction();

      // Check if the function exists
      if (fp == nullptr) {
        return;
      }

      // Check that it's a library call (called function must be empty)
      if (!(fp->empty())) {
        return;
      }

      // Check if it is an intrinsic function (e.g., llvm.debug etc.)
      if (fp->isIntrinsic()) {
        return;
      }

      // Get debug info (path to source file, variable name, line and column
      // numbers) and store them in an EntryInst
      DILocation *debugInfo = inst.getDebugLoc();
      if (debugInfo == nullptr) {
        errs() << "WARNING: instruction " << inst
          << " does not have debug information\n";
        return;
      }
      std::string directory = debugInfo->getDirectory().str();
      std::string fileName = debugInfo->getFilename().str();
      std::string pathToFile = std::string(directory + "/" + fileName);
      std::string varName = "";
      unsigned lineNum = debugInfo->getLine();
      unsigned columnNum = debugInfo->getColumn();
      Value *value = &inst;
      Value *allocPointer = nullptr;
      Value *allocNewPointer = nullptr;
      Value *allocSize = nullptr;
      Value *allocNumElems = nullptr;
      uint64_t size = 0;

      // Get the function call through the function name in the map
      std::string funcName = fp->getName().str();
      AllocFunctions val = AllocFunctions::_unknown;
      if (this->functionCalls.count(funcName)) {
        val = this->functionCalls[funcName];
      }

      // Switch to identify what function call is it, insert entry accordingly
      switch (val) {
        case AllocFunctions::_malloc:
        case AllocFunctions::_new: {
                                     allocPointer = &inst;
                                     allocSize = inst.getOperand(0);
                                     Entry *entry = new Entry(value, allocPointer, allocNewPointer, allocSize, allocNumElems, size, pathToFile, varName, lineNum, columnNum);
                                     this->allocations.push_back(entry);
                                   } break;

        case AllocFunctions::_mmap: {
                                      allocPointer = &inst;
                                      allocSize = inst.getOperand(1);
                                      Entry *entry = new Entry(value, allocPointer, allocNewPointer, allocSize, allocNumElems, size, pathToFile, varName, lineNum, columnNum);
                                      this->allocations.push_back(entry);
                                    } break;

        case AllocFunctions::_calloc: {
                                        allocPointer = &inst;
                                        allocSize = inst.getOperand(1);
                                        allocNumElems = inst.getOperand(0);
                                        Entry *entry = new Entry(value, allocPointer, allocNewPointer, allocSize, allocNumElems, size, pathToFile, varName, lineNum, columnNum);
                                        this->callocs.push_back(entry);
                                      } break;

        case AllocFunctions::_realloc: {
                                         allocPointer = inst.getOperand(0);
                                         allocNewPointer = &inst;
                                         allocSize = inst.getOperand(1);
                                         Entry *entry = new Entry(value, allocPointer, allocNewPointer, allocSize, allocNumElems, size, pathToFile, varName, lineNum, columnNum);
                                         this->reallocs.push_back(entry);
                                       } break;

        case AllocFunctions::_mremap: {
                                        allocPointer = inst.getOperand(0);
                                        allocNewPointer = &inst;
                                        allocSize = inst.getOperand(2);
                                        Entry *entry = new Entry(value, allocPointer, allocNewPointer, allocSize, allocNumElems, size, pathToFile, varName, lineNum, columnNum);
                                        this->reallocs.push_back(entry);
                                      } break;

        case AllocFunctions::_munmap:
        case AllocFunctions::_free:
        case AllocFunctions::_delete: {
                                        errs() << "dealloc inst visitor " << inst << "\n";
                                        this->deallocs.push_back(&inst);
                                      } break;

        default: {
                   //errs() << "WARNING: unknown function: " << funcName << "\n";
                   if (knowns.count(funcName)){ // It's a runtime function. TODO: use function pointer, rather than string
                     if (funcName == MEMORYTOOL_START_TRACKING){
                       this->callsToStartTracking.push_back(&inst);
                     }
                     break;
                   }
                   Entry *entry = new Entry(value, allocPointer, allocNewPointer, allocSize, allocNumElems, size, pathToFile, varName, lineNum, columnNum);
                   this->unknowns.push_back(entry);
                 }
      }

      // Note: this is here and not in visitInvokeInst because we want to filter the InvokeInst we care about
      if (InvokeInst *invokeInst = dyn_cast<InvokeInst>(&inst)) {
        this->invokes.push_back(invokeInst);
      }

      return;
    }

    // Visit all CallInst in a Module
    void visitCallInst(CallInst &inst) {
      visitCallInvokeInst(inst);

      return;
    }

    // Visit all InvokeInst in a Module
    void visitInvokeInst(InvokeInst &inst) {
      visitCallInvokeInst(inst);

      return;
    }

    // Visit all LoadInst in a Module
    void visitLoadInst(LoadInst &inst) {
      // Check if the inst can be skipped bacuse is not inside a ROI
      Function *instFunction = inst.getFunction();
      if (this->functionsToInstrumentLoadStore.count(instFunction) == 0){
        return;
      }

      // Check if the inst can be skipped thanks do DF analysis
      if (this->touchesToSkip.count(&inst)){
        return;
      }

      std::string pathToFile = "UnknownPathToFile";
      unsigned lineNum = 0;
      unsigned columnNum = 0;
      std::string varName = "";
      Value *value = &inst;
      Value *allocPointer = inst.getPointerOperand();
      Value *allocNewPointer = nullptr;
      Value *allocSize = nullptr;
      Value *allocNumElems = nullptr;
      uint64_t size = 0;

      // Get debug info to use in the instrumentation
      DILocation *debugInfo = inst.getDebugLoc();
      if (debugInfo != nullptr) {
        std::string directory = debugInfo->getDirectory().str();
        std::string fileName = debugInfo->getFilename().str();
        pathToFile = std::string(directory + "/" + fileName);
        lineNum = debugInfo->getLine();
        columnNum = debugInfo->getColumn();
      } else {
        errs() << "WARNING: store instruction " << inst
          << " does not have debug information\n";
      }

      Entry *entry = new Entry(value, allocPointer, allocNewPointer, allocSize, allocNumElems, size, pathToFile, varName, lineNum, columnNum);
      this->loads.push_back(entry);
      this->instToEntryMap[&inst] = entry;

      return;
    }

    // Visit all Escapes in a Module
    void visitEscapeInst(StoreInst &inst){
      // Checking to see if it is an escaping pointer reference
      if(inst.getValueOperand()->getType()->isPointerTy()){
        this->escapes.push_back(&inst);
      }
      return;
    }

    // Visit all StoreInst in a Module
    void visitStoreInst(StoreInst &inst) {
      visitEscapeInst(inst);

      // Check if the inst can be skipped bacuse is not inside a ROI
      Function *instFunction = inst.getFunction();
      if (this->functionsToInstrumentLoadStore.count(instFunction) == 0){
        return;
      }

      // Check if the inst can be skipped thanks do DF analysis
      if (this->touchesToSkip.count(&inst)){
        return;
      }

      std::string pathToFile = "UnknownPathToFile";
      unsigned lineNum = 0;
      unsigned columnNum = 0;
      std::string varName = "";
      Value *value = &inst;
      Value *allocPointer = inst.getPointerOperand();
      Value *allocNewPointer = nullptr;
      Value *allocSize = nullptr;
      Value *allocNumElems = nullptr;

      Value *storeValueOperand = inst.getValueOperand();
      Type *storeValueOperandType = storeValueOperand->getType();
      unsigned typeSizeInBits = storeValueOperandType->getScalarSizeInBits(); // store <2 x double> <double 1.000000e+00, double 1.000000e+00>, <2 x double>* %7, align 8, !dbg !387 size in bits 128
      uint64_t typeSizeInBytes = typeSizeInBits/8; // Size in bytes of each element
      unsigned primitiveSizeInBits = storeValueOperandType->getPrimitiveSizeInBits(); // store <2 x double> <double 1.000000e+00, double 1.000000e+00>, <2 x double>* %7, align 8, !dbg !387 size in bits 128
      uint64_t primitiveSizeInBytes = primitiveSizeInBits/8; // Size in bytes

      // Get debug info to use in the instrumentation
      DILocation *debugInfo = inst.getDebugLoc();
      if (debugInfo != nullptr) {
        std::string directory = debugInfo->getDirectory().str();
        std::string fileName = debugInfo->getFilename().str();
        pathToFile = std::string(directory + "/" + fileName);
        lineNum = debugInfo->getLine();
        columnNum = debugInfo->getColumn();
      } else {
        errs() << "WARNING: store instruction " << inst
          << " does not have debug information\n";
      }

      Entry *entry = new Entry(value, allocPointer, allocNewPointer, allocSize, allocNumElems, primitiveSizeInBytes, pathToFile, varName, lineNum, columnNum);
      entry->sizeElement = typeSizeInBytes;
      this->stores.push_back(entry);
      this->instToEntryMap[&inst] = entry;

      return;
    }

    // Visit all AllocaInst in a Module
    void visitAllocaInst(AllocaInst &inst) {
      // Check if this AllocaInst is part of the Functions to instrument
      Function *instFunction = inst.getFunction();
      if (this->functionsToInstrumentAllocaInst.count(instFunction) == 0){
        return;
      }

      uint64_t totalSizeInBytes = 0;
      Type *allocatedType = inst.getAllocatedType();
      // Array case
      if (allocatedType->isArrayTy()) {
        totalSizeInBytes = findArraySize(allocatedType);
      }
      // Struct case
      else if (allocatedType->isStructTy()) {
        totalSizeInBytes = findStructSize(allocatedType);
      }
      // Primitive
      else {
        totalSizeInBytes = allocatedType->getPrimitiveSizeInBits() / 8;
      }

      // Alloca is a pointer (TODO: figure out why the size is 0, shouldn't be 8
      // bytes for 64 bit machines?)
      if (totalSizeInBytes == 0) {
        Type *instType = inst.getType();
        if (instType->isPtrOrPtrVectorTy()) {
          // Cause assertion fail if allocatedType is not a pointer or a vector of
          // pointers
          Module *M = inst.getModule();
          auto dataLayout = M->getDataLayout();
          totalSizeInBytes = (uint64_t)dataLayout.getPointerTypeSize(instType);
        }
      }

      // Create fields for EntryInst
      std::string directory;
      std::string fileName;
      std::string pathToFile;
      std::string varName;
      unsigned lineNum;
      unsigned columnNum;
      uint64_t size = totalSizeInBytes;
      Value *value = &inst;
      Value *allocPointer = &inst;
      Value *allocNewPointer = nullptr;
      Value *allocSize = nullptr;
      Value *allocNumElems = nullptr;

      // Get debug info
      DILocation *debugInfo = inst.getDebugLoc();
      if (debugInfo ==
          nullptr) { // No direct debug info, look for llvm.dbg.declare
        errs() << "WARNING: instruction " << inst
          << " does not have debug information. Looking for "
          "llvm.dbg.declare ...\n";

        if (!inst.isUsedByMetadata()) {
          errs() << "WARNING: no isUsedByMetadata found for instruction " << inst
            << "\n";
          return;
        }

        auto *localAsMetadata = LocalAsMetadata::getIfExists(&inst);
        if (!localAsMetadata) {
          errs() << "WARNING: no LocalAsMetadata found for instruction " << inst
            << "\n";
          return;
        }

        auto *metadataAsValue =
          MetadataAsValue::getIfExists(inst.getContext(), localAsMetadata);
        if (!metadataAsValue) {
          errs() << "WARNING: no MetadataAsValue found for instruction " << inst
            << "\n";
          return;
        }

        // If there is more than one llvm.dbg.[addr|declare|value], give up, I
        // don't know which one to use (TODO: understand which one to use)
        if (!metadataAsValue->hasOneUse()) {
          errs()
            << "WARNING: alloca with more than 1 llvm.dbg.declare, instruction "
            << inst << " has " << metadataAsValue->getNumUses() << " uses\n";
          return;
        }

        // If we're here then there is only 1 llvm.dbg.declare
        DILocalVariable *localVariable = nullptr;
        for (User *user : metadataAsValue->users()) {
          if (DbgVariableIntrinsic *dbgDeclare =
              dyn_cast<DbgVariableIntrinsic>(user)) {
            localVariable = dbgDeclare->getVariable();
          }
        }

        if (!localVariable) {
          errs() << "ERROR: alloca cannot find llvm.dbg.declare, instruction "
            << inst << "\n";
          abort();
        }

        directory = localVariable->getDirectory().str();
        fileName = localVariable->getFilename().str();
        pathToFile = std::string(directory + "/" + fileName);
        varName = localVariable->getName();
        lineNum = localVariable->getLine();
        columnNum = 0;

      } else { // There are direct debung info connected to this alloca inst,
        // let's use them
        directory = debugInfo->getDirectory().str();
        fileName = debugInfo->getFilename().str();
        pathToFile = std::string(directory + "/" + fileName);
        varName = "UnavailableVariableName";
        lineNum = debugInfo->getLine();
        columnNum = debugInfo->getColumn();
      }

      Entry *entry = new Entry(value, allocPointer, allocNewPointer, allocSize, allocNumElems, size, pathToFile, varName, lineNum, columnNum);
      this->allocations.push_back(entry);

      // Add Alloca entry to functionAllocaMap too
      Function *parentFunc = inst.getFunction();
      this->functionAllocaMap[parentFunc].push_back(&inst);

      return;
    }

    void visitReturnInst(ReturnInst &inst) {
      this->returns.push_back(&inst);

      return;
    }
  };

  struct CAT : public ModulePass {

    static char ID;

    CAT() : ModulePass(ID) {}

    void getGlobals(Module &M, std::vector<Entry *> &allocations) {
      // This will go through the current global variables
      for (auto &global : M.getGlobalList()) {
        // Check if the global is valid
        if (!(&global)) {
          continue;
        }

        // All globals are pointer types apparently, but good sanity check
        if (global.getName() == "llvm.global_ctors") {
          continue;
        }

        uint64_t totalSizeInBytes = 0;
        // Each global variable can be either a struct, array, or a primitive
        if (global.getType()->isPointerTy()) {
          Type *valueType = global.getValueType();
          // Array case
          if (valueType->isArrayTy()) {
            totalSizeInBytes = findArraySize(valueType);
          }
          // Struct case
          else if (valueType->isStructTy()) {
            totalSizeInBytes = findStructSize(valueType);
          }
          // Primitive
          else {
            totalSizeInBytes = valueType->getPrimitiveSizeInBits() / 8;
          }

          // Global is a pointer (TODO: figure out why the size is 0, shouldn't be
          // 8 bytes for 64 bit machines?)
          if (totalSizeInBytes == 0) {
            Type *globalType = global.getType();
            if (globalType->isPtrOrPtrVectorTy()) {
              // Cause assertion fail if allocatedType is not a pointer or a
              // vector of pointers
              auto dataLayout = M.getDataLayout();
              totalSizeInBytes =
                (uint64_t)dataLayout.getPointerTypeSize(globalType);
            }
          }

          // Get debug info (path to source file, variable name, line and column
          // numbers) and store them in an EntryGlobal
          SmallVector<DIGlobalVariableExpression *, 1> globalDebugInfoAll;
          global.getDebugInfo(globalDebugInfoAll);
          if (globalDebugInfoAll.size() == 0) {
            errs() << "WARNING: global varialbe " << global
              << " does not have debug information.\n";
            continue;
          }

          auto *debugInfo = globalDebugInfoAll[0]->getVariable();
          if (debugInfo == nullptr) {
            errs() << "WARNING: global varialbe " << global
              << " does not have debug information.\n";
            continue;
          }

          // At this point we have debug info
          std::string fileName = debugInfo->getFilename().str();
          std::string directory = debugInfo->getDirectory().str();
          std::string pathToFile = std::string(directory + "/" + fileName);
          std::string varName = debugInfo->getName().str();
          unsigned lineNum = debugInfo->getLine();
          unsigned columnNum =
            0; // apparently there is no column number for globals
          uint64_t size = totalSizeInBytes;
          Value *value = &global;
          Value *allocPointer = &global;
          Value *allocNewPointer = nullptr;
          Value *allocSize = nullptr;
          Value *allocNumElems = nullptr;

          Entry *entry = new Entry(value, allocPointer, allocNewPointer, allocSize, allocNumElems, size, pathToFile, varName, lineNum, columnNum);

          allocations.push_back(entry);
        }
      }

      return;
    }

    // Insert runtime functions into current module
    bool insertRuntimeFunctions(Module &M) {
      // Get the llvm ir data type we need
      LLVMContext &TheContext = M.getContext();
      Type *int8PtrType = Type::getInt8PtrTy(TheContext);
      Type *int64Type = Type::getInt64Ty(TheContext);
      Type *voidType = Type::getVoidTy(TheContext);

      // Alloc
      std::vector<Type *> paramsAlloc = {int64Type, int8PtrType, int64Type, int8PtrType};
      ArrayRef<Type *> argsAlloc(paramsAlloc);
      FunctionType *signatureAlloc =
        FunctionType::get(voidType, argsAlloc, false);
      M.getOrInsertFunction(TEXAS_MALLOC, signatureAlloc);

      // Calloc
      std::vector<Type *> paramsCalloc = {int64Type, int8PtrType, int64Type, int64Type, int8PtrType};
      ArrayRef<Type *> argsCalloc(paramsCalloc);
      FunctionType *signatureCalloc =
        FunctionType::get(voidType, argsCalloc, false);
      M.getOrInsertFunction(TEXAS_CALLOC, signatureCalloc);

      // Realloc
      std::vector<Type *> paramsRealloc = {int64Type, int8PtrType, int8PtrType, int64Type, int8PtrType};
      ArrayRef<Type *> argsRealloc(paramsRealloc);
      FunctionType *signatureRealloc =
        FunctionType::get(voidType, argsRealloc, false);
      M.getOrInsertFunction(TEXAS_REALLOC, signatureRealloc);

      // Escape
      std::vector<Type *> paramsEscape = {int8PtrType};
      ArrayRef<Type *> argsAddEscape(paramsEscape);
      FunctionType *signatureAddEscape = 
        FunctionType::get(voidType, argsAddEscape, false);
      M.getOrInsertFunction(TEXAS_ADD_ESCAPE, signatureAddEscape);

      // Remove and Add
      std::vector<Type *> paramsAddRemove = {int8PtrType};
      ArrayRef<Type *> argsAddRemove(paramsAddRemove);
      FunctionType *signatureAddRemove =
        FunctionType::get(voidType, argsAddRemove, false);
      M.getOrInsertFunction(TEXAS_ADD, signatureAddRemove);
      M.getOrInsertFunction(TEXAS_REMOVE, signatureAddRemove);

      // AddWithInfo Store
      std::vector<Type *> paramsAddWithInfoStore = {int64Type, int8PtrType, int64Type, int64Type, int8PtrType};
      ArrayRef<Type *> argsAddWithInfoStore(paramsAddWithInfoStore);
      FunctionType *signatureAddWithInfoStore =
        FunctionType::get(voidType, argsAddWithInfoStore, false);
      M.getOrInsertFunction(TEXAS_ADD_WITH_INFO, signatureAddWithInfoStore);

      // AddWithInfo Load
      std::vector<Type *> paramsAddWithInfoLoad = {int64Type, int8PtrType, int8PtrType};
      ArrayRef<Type *> argsAddWithInfoLoad(paramsAddWithInfoLoad);
      FunctionType *signatureAddWithInfoLoad =
        FunctionType::get(voidType, argsAddWithInfoLoad, false);
      M.getOrInsertFunction(TEXAS_ADD_WITH_INFO_LOAD, signatureAddWithInfoLoad);

      // Set FSA state
      std::vector<Type *> paramsSetState = {int64Type, int8PtrType, int64Type, int64Type, int64Type, int64Type, int8PtrType};
      ArrayRef<Type *> argsSetState(paramsSetState);
      FunctionType *signatureSetState =
        FunctionType::get(voidType, argsSetState, false);
      M.getOrInsertFunction(TEXAS_SET_STATE, signatureSetState);

      // Callstack
      FunctionType *signatureCallstack =
        FunctionType::get(int8PtrType, false);
      M.getOrInsertFunction(MEMORYTOOL_CALLSTACK, signatureCallstack);

      // Callstack unique
      std::vector<Type *> paramsCallstackUnique = {int64Type, int64Type};
      ArrayRef<Type *> argsCallstackUnique(paramsCallstackUnique);
      FunctionType *signatureCallstackUnique =
        FunctionType::get(int8PtrType, argsCallstackUnique, false);
      M.getOrInsertFunction(MEMORYTOOL_CALLSTACK_UNIQUE, signatureCallstackUnique);

      // Remove Callstack
      FunctionType *signatureRmCallstack = FunctionType::get(voidType, false);
      M.getOrInsertFunction(MEMORYTOOL_RMCALLSTACK, signatureRmCallstack);

      // Pin start stop tracking
      FunctionType *signaturePin = FunctionType::get(voidType, false);
      M.getOrInsertFunction(PIN_START_TRACKING, signaturePin);
      M.getOrInsertFunction(PIN_STOP_TRACKING, signaturePin);

      // Pin add
      std::vector<Type *> paramsPinAdd = {int64Type, int8PtrType};
      FunctionType *signaturePinAdd =
        FunctionType::get(voidType, paramsPinAdd, false);
      M.getOrInsertFunction(PIN_ADD, signaturePinAdd);

      // Prologue
      std::vector<Type *> paramsPrologue = {int64Type, int64Type, int64Type, int64Type, int64Type, int64Type, int64Type, int64Type};
      ArrayRef<Type *> argsPrologue(paramsPrologue);
      FunctionType *signaturePrologue = FunctionType::get(voidType, argsPrologue, false);
      M.getOrInsertFunction(MEMORYTOOL_PROLOGUE, signaturePrologue);

      // Epilogue
      FunctionType *signatureEpilogue = FunctionType::get(voidType, false);
      M.getOrInsertFunction(MEMORYTOOL_EPILOGUE, signatureEpilogue);

      return true;
    }


    bool insertRmCallstackInstrumentation(Module &M, std::vector<ReturnInst*> &returnInsts, std::vector<InvokeInst*> &invokeInsts){
      Function *runtimeFunc = getRuntimeFunction(M, MEMORYTOOL_RMCALLSTACK);

      for (auto returnInst : returnInsts){
        Instruction *insertPoint = returnInst;
        CallInst *rmCallstack = CallInst::Create(runtimeFunc);
        rmCallstack->insertBefore(insertPoint);
      }

      for (auto invokeInst : invokeInsts){
        Instruction *insertPoint = &(invokeInst->getUnwindDest()->back());
        CallInst *rmCallstack = CallInst::Create(runtimeFunc);
        rmCallstack->insertBefore(insertPoint);
      }

      return true;
    }

    // Add calls to runtime for deallocations of alloca (remove alloca for invoke
    // inst unwind label)
    bool
      instrumentInvokes(Module &M, std::vector<InvokeInst *> &insts,
          std::unordered_map<Function *, std::vector<Instruction *>>
          &functionAllocaMap) {
        bool modified = false;

        // There are no instructions to instrument, return false
        if (insts.size() <= 0) {
          return modified;
        }

        // Get the llvm ir data type we need
        LLVMContext &TheContext = M.getContext();
        Type *voidPointerType = Type::getInt8PtrTy(TheContext, 0);
        Type *int64Type = Type::getInt64Ty(TheContext);

        // Get the runtime function
        Function *runtimeFunc = getRuntimeFunction(M, TEXAS_REMOVE);

        // Get some default values we need later on
        Instruction *tempI = nullptr;
        const Twine &NameStr = "";

        // Go through instructions
        for (auto *inst : insts) {
          // Get the function the return inst is in
          Function *currentFunc = inst->getFunction();
          if (functionAllocaMap.count(currentFunc) == 0) {
            errs() << "WARNING: the function " << currentFunc->getName()
              << " of return inst " << *inst
              << " is not in functionAllocaMap\n"; // it's a warning because a
            // function might not have
            // alloca instructions (no
            // locals...is that even
            // possible?)
            continue;
          }

          Instruction &insertPoint = inst->getUnwindDest()->back();

          // Iterate over the alloca inst of the function this return is in
          for (auto allocaInst : functionAllocaMap[currentFunc]) {
            // Get current inst debug location
            const DebugLoc &instDebugLoc = getDefaultDebugLoc(allocaInst);

            // Pointer cast for address that is beeing freed or deleted
            CastInst *pointerCast = CastInst::CreatePointerCast(
                allocaInst, voidPointerType, NameStr, tempI);
            pointerCast->setDebugLoc(instDebugLoc);

            // Create call instruction to runtime
            std::vector<Value *> callVals;
            callVals.push_back(pointerCast);
            ArrayRef<Value *> callArgs = ArrayRef<Value *>(callVals);
            CallInst *rmFromAllocationTable =
              CallInst::Create(runtimeFunc, callArgs, NameStr, tempI);
            rmFromAllocationTable->setDebugLoc(instDebugLoc);

            // Add instructions to current module (TODO: consider to use an
            // IRBuilder)
            pointerCast->insertBefore(&insertPoint); // address to be freed cast
            rmFromAllocationTable->insertAfter(
                pointerCast); // function call to runtime

            // At this point we have modified the IR
            modified = true;
          }
        }

        return modified;
      }

    // Add calls to runtime for deallocations of alloca (before every return inst)
    bool
      instrumentReturns(Module &M, std::vector<ReturnInst *> &insts,
          std::unordered_map<Function *, std::vector<Instruction *>>
          &functionAllocaMap) {
        bool modified = false;

        // There are no instructions to instrument, return false
        if (insts.size() <= 0) {
          return modified;
        }

        // Get the llvm ir data type we need
        LLVMContext &TheContext = M.getContext();
        Type *voidPointerType = Type::getInt8PtrTy(TheContext, 0);
        Type *int64Type = Type::getInt64Ty(TheContext);

        // Get the runtime function
        Function *runtimeFunc = getRuntimeFunction(M, TEXAS_REMOVE);

        // Get some default values we need later on
        Instruction *tempI = nullptr;
        const Twine &NameStr = "";

        // Go through instructions
        for (auto *inst : insts) {
          // Get the function the return inst is in
          Function *currentFunc = inst->getFunction();
          if (functionAllocaMap.count(currentFunc) == 0) {
            errs() << "WARNING: the function " << currentFunc->getName()
              << " of return inst " << *inst
              << " is not in functionAllocaMap\n"; // it's a warning because a
            // function might not hae
            // alloca insttructions (no
            // locals...is that even
            // possible?)
            continue;
          }

          // Iterate over the alloca inst of the function this return is in
          for (auto allocaInst : functionAllocaMap[currentFunc]) {
            // Get current inst debug location
            const DebugLoc &instDebugLoc = getDefaultDebugLoc(allocaInst);

            // Pointer cast for address that is beeing freed or deleted
            CastInst *pointerCast = CastInst::CreatePointerCast(
                allocaInst, voidPointerType, NameStr, tempI);
            pointerCast->setDebugLoc(instDebugLoc);

            // Create call instruction to runtime
            std::vector<Value *> callVals;
            callVals.push_back(pointerCast);
            ArrayRef<Value *> callArgs = ArrayRef<Value *>(callVals);
            CallInst *rmFromAllocationTable =
              CallInst::Create(runtimeFunc, callArgs, NameStr, tempI);
            rmFromAllocationTable->setDebugLoc(instDebugLoc);

            // Add instructions to current module (TODO: consider to use an
            // IRBuilder)
            pointerCast->insertBefore(inst); // address to be freed cast
            rmFromAllocationTable->insertAfter(
                pointerCast); // function call to runtime

            // At this point we have modified the IR
            modified = true;
          }
        }

        return modified;
      }

    // Remove deallocations (i.e., free and delete)
    bool eraseDeallocs(std::vector<Instruction *> &deallocs) {
      bool modified = false;

      // There are no mallocs to instrument, return false
      if (deallocs.size() <= 0) {
        return modified;
      }

      // Go through instructions and remove them
      for (auto inst : deallocs) {
        errs() << "dealloc " << *inst << "\n";
        inst->eraseFromParent();
      }

      // At this point we have modified the IR
      modified = true;

      return modified;
    }

    // Add calls to runtime for deallocations (i.e., free and delete)
    bool instrumentDeallocs(Module &M, std::vector<Instruction *> &deallocs) {
      bool modified = false;

      // There are no mallocs to instrument, return false
      if (deallocs.size() <= 0) {
        return modified;
      }

      // Get the llvm ir data type we need
      LLVMContext &TheContext = M.getContext();
      Type *voidPointerType = Type::getInt8PtrTy(TheContext, 0);
      Type *int64Type = Type::getInt64Ty(TheContext);

      // Get the runtime function
      Function *allocFunc = getRuntimeFunction(M, TEXAS_REMOVE);

      // Get some default values we need later on
      Instruction *tempI = nullptr;
      const Twine &NameStr = "";

      // Go through instructions
      for (auto *inst : deallocs) {
        // Get current inst debug location
        const DebugLoc &instDebugLoc = getDefaultDebugLoc(inst);

        // Pointer cast for address that is beeing freed or deleted
        CastInst *pointerCast = CastInst::CreatePointerCast(
            inst->getOperand(0), voidPointerType, NameStr, tempI);
        pointerCast->setDebugLoc(instDebugLoc);

        // Create call instruction to runtime
        std::vector<Value *> callVals;
        callVals.push_back(pointerCast);
        ArrayRef<Value *> callArgs = ArrayRef<Value *>(callVals);
        CallInst *addToAllocationTable =
          CallInst::Create(allocFunc, callArgs, NameStr, tempI);
        addToAllocationTable->setDebugLoc(instDebugLoc);

        // Add instructions to current module (TODO: consider to use an IRBuilder)
        pointerCast->insertAfter(inst); // address to be freed cast
        addToAllocationTable->insertAfter(
            pointerCast); // function call to runtime

        // At this point we have modified the IR
        modified = true;
      }

      return modified;
    }

    // Add calls to runtime when the state gets modified (i.e., store inst)
    bool instrumentEscapes(Module &M, std::vector<StoreInst *> &escapes) {
      bool modified = false;

      // There are no stores to instrument, return false
      if (escapes.size() <= 0) {
        return modified;
      }

      // Get the llvm ir data type we need
      LLVMContext &TheContext = M.getContext();
      Type *voidPointerType = Type::getInt8PtrTy(TheContext, 0);

      // Get the runtime function
      Function *addFunc = getRuntimeFunction(M, TEXAS_ADD_ESCAPE);

      // Get some default values we need later on
      Instruction *tempI = nullptr;
      const Twine &NameStr = "";

      for (auto entry : escapes) {
        // Get store inst
        StoreInst *storeInst = dyn_cast<StoreInst>(entry);
        if (storeInst == nullptr) {
          errs() << "ERROR: instruction " << *(entry)
            << " is not a StoreInst\n";
          abort();
        }

        // Create store pointer to pass as an arg to the runtime
        CastInst *pointerCast = CastInst::CreatePointerCast(
            storeInst->getPointerOperand(), voidPointerType, NameStr, tempI);

        // Create call instruction to runtime
        std::vector<Value *> callVals;
        callVals.push_back(pointerCast);
        ArrayRef<Value *> callArgs = ArrayRef<Value *>(callVals);
        CallInst *addEscape =
          CallInst::Create(addFunc, callArgs, NameStr, tempI);

        // Add instructions to current module (TODO: consider to use an IRBuilder)
        std::vector<Instruction *> instsToInsert = {
          pointerCast, addEscape};
        Instruction *insertPoint = storeInst;
        insertInstructionsWithCheck(instsToInsert, insertPoint);

        // At this point we have modified the IR
        modified = true;
      }

      return modified;
    }

    bool instrumentLoopMostlyInvariantsToRemove(
        Module &M, std::map<Function *, Instruction *> &funcToCallstack,
        std::unordered_map<Instruction *, Entry *> &instToEntryMap,
        std::unordered_set<Instruction *> instsToInstrument) {
      bool modified = false;

      // If there are no stores to instrument, just return.
      if (instsToInstrument.size() == 0) {
        return false;
      }

      // Get things we need.
      LLVMContext &context = M.getContext();
      Type *int64Type = Type::getInt64Ty(context);
      Twine emptyName = Twine("");
      DataLayout dataLayout(&M);

      for (auto I : instsToInstrument) {
        if (instToEntryMap.count(I) == 0){
          errs() << "ERROR: instruction " << *I << " does not have entry. Abort.\n";
          abort();
        }

        Entry *entry = instToEntryMap[I];
        Value *addr = entry->allocPointer;

        // Add a stack location for old value of pointer to first BB and counter
        Type *storePointerType = addr->getType();
        if (!storePointerType->isPointerTy()){
          errs() << "ERROR: this " << *storePointerType << " is not a pointer type. Abort.\n";
          abort();
        }
        PointerType *pointerType = dyn_cast<PointerType>(storePointerType);
        auto parentFunc = I->getFunction();
        auto &firstBB = parentFunc->getEntryBlock();
        Instruction *insertPointAlloca = &(*(firstBB.getFirstInsertionPt()));

        ConstantPointerNull *nullptrConstant = ConstantPointerNull::get(pointerType);
        GlobalVariable *newAllocation = new GlobalVariable(M, pointerType, false, GlobalValue::CommonLinkage, nullptrConstant);
        //AllocaInst* newAllocation = new AllocaInst(storePointerType, 0, emptyName, insertPointAlloca);
        //AllocaInst* counterAllocation = new AllocaInst(int64Type, 0, emptyName, newAllocation);
        Constant *counterConstant0 = ConstantInt::get(int64Type, 0);
        GlobalVariable *counterAllocation = new GlobalVariable(M, int64Type, false, GlobalValue::CommonLinkage, counterConstant0);

        // Fill the new alloca with 0 the first time.
        //APInt value0(storePointerType->getPointerElementType()->getPrimitiveSizeInBits(), 0);
        //Constant *constant0 = Constant::getIntegerValue(storePointerType, value0);
        //APInt value0(dataLayout.getPointerTypeSizeInBits(storePointerType), 0);
        //Constant *constant0 = Constant::getIntegerValue(storePointerType, value0);
        //StoreInst* allocInit = new StoreInst(constant0, newAllocation, false);
        //allocInit->insertAfter(newAllocation);

        //Constant *counterConstant0 = ConstantInt::get(int64Type, 0);
        //StoreInst *counterInit = new StoreInst(counterConstant0, counterAllocation, false);
        //counterInit->insertAfter(counterAllocation);

        // Split the store inst basic block in two.
        // The first instruction of the new basic block is the splitting instruction (i.e., store inst in our case).
        BasicBlock* oldBlock = I->getParent();
        BasicBlock* newBlock = oldBlock->splitBasicBlock(I, "");
        BasicBlock* wrapperBlock = BasicBlock::Create(context, emptyName, parentFunc, newBlock);

        // At the end of the old basic block compare the store pointer with alloca (containing the old store pointer value).
        // Remove the last instruction from oldBlock.
        oldBlock->getTerminator()->removeFromParent();
        LoadInst *counterLoad = new LoadInst(int64Type, counterAllocation, emptyName, oldBlock);
        Constant *constant2 = ConstantInt::get(int64Type, 2);
        ICmpInst *cmpInstLT2 = new ICmpInst(*oldBlock, ICmpInst::ICMP_ULT, counterLoad, constant2);
        // Branch to store BB
        BranchInst* brFromLT2 = BranchInst::Create(wrapperBlock, newBlock, cmpInstLT2, oldBlock);

        // Populate the wrapper block.
        // Add 1 to counter
        LoadInst *counterLoadAgain = new LoadInst(int64Type, counterAllocation, emptyName, wrapperBlock);
        Constant *constant1 = ConstantInt::get(int64Type, 1);
        BinaryOperator *add1 = BinaryOperator::Create(Instruction::Add, counterLoadAgain, constant1, emptyName, wrapperBlock);
        // Store new counter value to counter
        StoreInst *counterUpdate = new StoreInst(add1, counterAllocation, wrapperBlock);
        // Get instrumentation.
        LoadStoreInstrumentation *loadStoreInstrumentation =
          getLoadStoreInstrumentation(M, entry, funcToCallstack);
        // Inject branch to newBlock.
        BranchInst *gotoNewBlock = BranchInst::Create(newBlock, wrapperBlock);
        // Insert instrumentation in wrapper basic block.
        std::vector<Instruction *> instsToInsert = {loadStoreInstrumentation->pointerCast, loadStoreInstrumentation->addToState};
        Instruction *insertPoint = gotoNewBlock;
        insertInstructionsWithCheck(instsToInsert, insertPoint);

        // If we are here we modified the bitcode
        modified = true;
      }

      return modified;
    }

    bool instrumentLoadsStoresToRemove(Module &M, std::map<Function*, Instruction*> &funcToCallstack, std::unordered_map<Instruction*, Entry*> &instToEntryMap, std::unordered_set<Instruction*> &alreadyInstrumented){
      bool modified = false;

      // Gather load and stores left to instrument
      std::unordered_set<Instruction*> toInstrument;
      for (auto elem : instToEntryMap) {
        Instruction *inst = elem.first;

        if (alreadyInstrumented.count(inst)){ // This load/store has already been instrumented
          continue;
        }

        toInstrument.insert(inst);
      }

      modified |= instrumentLoopMostlyInvariantsToRemove(M, funcToCallstack, instToEntryMap, toInstrument);

      return modified;
    }

    bool instrumentLoadsStores(Module &M, std::map<Function*, Instruction*> &funcToCallstack, std::unordered_map<Instruction*, Entry*> &instToEntryMap, std::unordered_set<Instruction*> &alreadyInstrumented){
      bool modified = false;

      // Gather load and stores left to instrument
      std::unordered_set<Instruction*> toInstrument;
      for (auto elem : instToEntryMap) {
        Instruction *inst = elem.first;

        if (alreadyInstrumented.count(inst)){ // This load/store has already been instrumented
          continue;
        }

        toInstrument.insert(inst);
      }

      modified |= instrumentLoopMostlyInvariants(M, funcToCallstack, instToEntryMap, toInstrument);

      return modified;
    }

    bool instrumentUnkowns(Module &M, std::vector<Entry *> &entries, std::map<Function*, Instruction*> &funcToCallstack) {
      bool modified = false;

      // There are no insts to instrument, return false
      if (entries.size() <= 0) {
        return modified;
      }

      // Get the llvm ir data type we need
      LLVMContext &TheContext = M.getContext();
      Type *voidPointerType = Type::getInt8PtrTy(TheContext, 0);
      Type *int64Type = Type::getInt64Ty(TheContext);

      // Get the runtime function
      Function *start = getRuntimeFunction(M, PIN_START_TRACKING);
      Function *stop = getRuntimeFunction(M, PIN_STOP_TRACKING);
      Function *add = getRuntimeFunction(M, PIN_ADD);

      // Get some default values we need later on
      Instruction *tempI = nullptr;
      const Twine &NameStr = "";

      for (auto entry : entries) {
        // Get inst
        CallBase *inst = dyn_cast<CallBase>(entry->value);
        if (inst == nullptr) {
          errs() << "ERROR: instruction " << *(entry->value)
            << " is not a CallBaseInst\n";
          abort();
        }

        // Get current inst debug location
        const DebugLoc &instDebugLoc = getDefaultDebugLoc(inst);

        // Create call to start tracking and insert it before current inst
        CallInst *callToStart =
          CallInst::Create(start, NameStr, inst);
        callToStart->setDebugLoc(instDebugLoc);

        // Create call to stop tracking
        CallInst *callToStop =
          CallInst::Create(stop);
        callToStop->setDebugLoc(instDebugLoc);

        // ID
        uint64_t id = entry->id;
        APInt IDValue(64, id);
        Constant *IDConstant =
          Constant::getIntegerValue(int64Type, IDValue);

        // Create call to add
        std::vector<Value *> callVals;
        callVals.push_back(IDConstant);

        Instruction *insertPoint = nullptr;
        if (Instruction *inst = dyn_cast<Instruction>(entry->value)){ // It's an instruction
          // Safety check for callstack
          Function* currInstFunc = inst->getFunction();
          if (funcToCallstack.count(currInstFunc) == 0){
            errs() << "ERROR: no callstack for function " << currInstFunc->getName() << "\n"; 
            abort();
          }
          Instruction *callstackInst = funcToCallstack[currInstFunc];
          callVals.push_back(callstackInst);
          insertPoint = inst;

        } else {
          errs() << "ERROR: instruction " << *(entry->value) << " is not an Instruction. Abort.\n";
          abort();
        }

        ArrayRef<Value *> callArgs = ArrayRef<Value *>(callVals);
        CallInst *callToAdd =
          CallInst::Create(add, callArgs);
        callToAdd->setDebugLoc(instDebugLoc);

        // Add instructions to current module (TODO: consider to use an IRBuilder)
        std::vector<Instruction *> instsToInsert = {callToStop, callToAdd};
        insertInstructionsWithCheck(instsToInsert, insertPoint);

        // At this point we have modified the IR
        modified = true;
      }

      return modified;
    }

    bool instrumentLoadsStoresRegular(Module &M, std::map<Function*, Instruction*> &funcToCallstack, std::unordered_map<Instruction*, Entry*> &instToEntryMap, std::unordered_set<Instruction*> &alreadyInstrumented){
      bool modified = false;
      for (auto elem : instToEntryMap) {
        Instruction *inst = elem.first;

        if (alreadyInstrumented.count(inst)){ // This load/store has already been instrumented
          continue;
        }

        Entry *entry = elem.second;
        LoadStoreInstrumentation *loadStoreInstrumentation = getLoadStoreInstrumentation(M, entry, funcToCallstack);

        // Insert instrumentation
        std::vector<Instruction *> instsToInsert = {loadStoreInstrumentation->pointerCast, loadStoreInstrumentation->addToState};
        Instruction *insertPoint = inst;
        insertInstructionsWithCheck(instsToInsert, insertPoint);

        // At this point we have modified the IR
        modified = true;
      }

      return modified;
    }

    bool changeGlobalsPriority(Module &M) {
      bool modified = false;

      // Get the types we need
      LLVMContext &TheContext = M.getContext();
      Type *int32Type = Type::getInt32Ty(TheContext);

      // New priority: 65535 is the default priority, the lower the number the
      // higher the priority, so we add 1 to target program globals
      uint64_t newPriority = 65535 + 1;

      // Get llvm.global_ctors (list of constructors for global variables)
      GlobalVariable *globalCtors = M.getNamedGlobal("llvm.global_ctors");

      // Check if llvm.global_ctors exists (e.g., it does not exist if there is no
      // global variable)
      if (globalCtors == nullptr) {
        return modified;
      }

      // Get the initializer (it's an array)
      Constant *initializer = globalCtors->getInitializer();

      // Iterate over the globals constructors
      for (auto i = 0; i < initializer->getNumOperands(); ++i) {
        // Get each global constructor
        if (ConstantStruct *element =
            dyn_cast<ConstantStruct>(initializer->getOperand(i))) {
          // Change its priority, so it gets called after the global constructors
          // of our runtime
          APInt priorityValue(32, (uint32_t)newPriority);
          Constant *priorityConstant =
            Constant::getIntegerValue(int32Type, priorityValue);
          element->setOperand(0, priorityConstant);

          // At this point we have modified the IR
          modified = true;
        } else {
          errs() << "ERROR: initializer " << *(initializer)
            << " is not a ConstantStruct.\n";
          abort();
        }
      }

      return modified;
    }

    bool insertPrologue(Module &M) {
      bool modified = false;

      Function *prologueFunc = getRuntimeFunction(M, MEMORYTOOL_PROLOGUE);
      appendToGlobalCtors(M, prologueFunc, 65536);
      modified = true;

      return modified;
    }


    // Add calls to runtime for reallocs
    bool instrumentReallocs(Module &M, std::vector<Entry*> &entries, std::map<Function*, Instruction*> &funcToCallstack) {
      bool modified = false;

      // There are no mallocs to instrument, return false
      if (entries.size() <= 0) {
        return modified;
      }

      // Get the llvm ir data type we need
      LLVMContext &TheContext = M.getContext();
      Type *voidPointerType = Type::getInt8PtrTy(TheContext, 0);
      Type *int8Type = Type::getInt8Ty(TheContext);
      Type *int64Type = Type::getInt64Ty(TheContext);

      // Get the runtime function
      Function *runtimeFunc = getRuntimeFunction(M, TEXAS_REALLOC);

      // Get some default values we need later on
      Instruction *tempI = nullptr;
      const Twine &NameStr = "";

      // Go through instructions
      for (auto entry : entries) {

        // Get current inst debug location
        const DebugLoc &instDebugLoc = getDefaultDebugLoc(entry->value);

        // Pointer cast for address
        CastInst *pointerCast = CastInst::CreatePointerCast(
            entry->allocPointer, voidPointerType, NameStr, tempI);
        pointerCast->setDebugLoc(instDebugLoc);

        // Pointer cast for new address
        CastInst *newPointerCast = CastInst::CreatePointerCast(
            entry->allocNewPointer, voidPointerType, NameStr, tempI);
        newPointerCast->setDebugLoc(instDebugLoc);

        // Pointer cast for elem size
        CastInst *int64CastSize = CastInst::CreateZExtOrBitCast(entry->allocSize,
            int64Type, NameStr, tempI);
        int64CastSize->setDebugLoc(instDebugLoc);

        // ID
        uint64_t id = entry->id;
        APInt IDValue(64, id);
        Constant *IDConstant =
          Constant::getIntegerValue(int64Type, IDValue);

        // Create call instruction to runtime
        std::vector<Value *> callVals;
        callVals.push_back(IDConstant);
        callVals.push_back(pointerCast);
        callVals.push_back(newPointerCast);
        callVals.push_back(int64CastSize);

        Instruction *insertPoint = nullptr;
        if (Instruction *inst = dyn_cast<Instruction>(entry->value)){ // It's an instruction
          // Safety check for callstack
          Function* currInstFunc = inst->getFunction();
          if (funcToCallstack.count(currInstFunc) == 0){
            errs() << "ERROR: no callstack for function " << currInstFunc->getName() << "\n"; 
            abort();
          }
          Instruction *callstackInst = funcToCallstack[currInstFunc];
          callVals.push_back(callstackInst);
          insertPoint = inst;

        } else {
          errs() << "ERROR: instruction " << *(entry->value) << " is not and Instruction nor a GlobalVariable. Abort.\n";
          abort();
        }

        ArrayRef<Value *> callArgs = ArrayRef<Value *>(callVals);
        CallInst *callToRuntime =
          CallInst::Create(runtimeFunc, callArgs);
        callToRuntime->setDebugLoc(instDebugLoc);

        std::vector<Instruction *> instsToInsert = {pointerCast, newPointerCast, int64CastSize, callToRuntime};
        insertInstructionsWithCheck(instsToInsert, insertPoint);

        // At this point we have modified the IR
        modified = true;
      }

      return modified;
    }

    // Add calls to runtime for callocs
    bool instrumentCallocs(Module &M, std::vector<Entry*> &entries, std::map<Function*, Instruction*> &funcToCallstack) {
      bool modified = false;

      // There are no mallocs to instrument, return false
      if (entries.size() <= 0) {
        return modified;
      }

      // Get the llvm ir data type we need
      LLVMContext &TheContext = M.getContext();
      Type *voidPointerType = Type::getInt8PtrTy(TheContext, 0);
      Type *int8Type = Type::getInt8Ty(TheContext);
      Type *int64Type = Type::getInt64Ty(TheContext);

      // Get the runtime function
      Function *runtimeFunc = getRuntimeFunction(M, TEXAS_CALLOC);

      // Get some default values we need later on
      Instruction *tempI = nullptr;
      const Twine &NameStr = "";

      // Go through instructions
      for (auto entry : entries) {

        // Get current inst debug location
        const DebugLoc &instDebugLoc = getDefaultDebugLoc(entry->value);

        // Pointer cast for address
        CastInst *pointerCast = CastInst::CreatePointerCast(
            entry->allocPointer, voidPointerType, NameStr, tempI);
        pointerCast->setDebugLoc(instDebugLoc);

        // Pointer cast for size
        CastInst *int64CastSize = CastInst::CreateZExtOrBitCast(entry->allocSize,
            int64Type, NameStr, tempI);
        int64CastSize->setDebugLoc(instDebugLoc);

        // Pointer cast for elem size
        CastInst *int64CastNumElem = CastInst::CreateZExtOrBitCast(entry->allocNumElems,
            int64Type, NameStr, tempI);
        int64CastNumElem->setDebugLoc(instDebugLoc);

        // ID
        uint64_t id = entry->id;
        APInt IDValue(64, id);
        Constant *IDConstant =
          Constant::getIntegerValue(int64Type, IDValue);

        // Create call instruction to runtime
        std::vector<Value *> callVals;
        callVals.push_back(IDConstant);
        callVals.push_back(pointerCast);
        callVals.push_back(int64CastSize);
        callVals.push_back(int64CastNumElem);

        Instruction *insertPoint = nullptr;
        if (Instruction *inst = dyn_cast<Instruction>(entry->value)){ // It's an instruction
          // Safety check for callstack
          Function* currInstFunc = inst->getFunction();
          if (funcToCallstack.count(currInstFunc) == 0){
            errs() << "ERROR: no callstack for function " << currInstFunc->getName() << "\n"; 
            abort();
          }
          Instruction *callstackInst = funcToCallstack[currInstFunc];
          callVals.push_back(callstackInst);
          insertPoint = inst;

        } else {
          errs() << "ERROR: instruction " << *(entry->value) << " is not and Instruction nor a GlobalVariable. Abort.\n";
          abort();
        }

        ArrayRef<Value *> callArgs = ArrayRef<Value *>(callVals);
        CallInst *callToRuntime =
          CallInst::Create(runtimeFunc, callArgs);
        callToRuntime->setDebugLoc(instDebugLoc);

        std::vector<Instruction *> instsToInsert = {pointerCast, int64CastSize, int64CastNumElem, callToRuntime};
        insertInstructionsWithCheck(instsToInsert, insertPoint);

        // At this point we have modified the IR
        modified = true;
      }

      return modified;
    }


    // Add calls to runtime for mallocs
    bool instrumentAllocations(Module &M, std::vector<Entry*> &entries, std::map<Function*, Instruction*> &funcToCallstack) {
      bool modified = false;

      // There are no mallocs to instrument, return false
      if (entries.size() <= 0) {
        return modified;
      }

      // Get the llvm ir data type we need
      LLVMContext &TheContext = M.getContext();
      Type *voidPointerType = Type::getInt8PtrTy(TheContext, 0);
      Type *int8Type = Type::getInt8Ty(TheContext);
      Type *int64Type = Type::getInt64Ty(TheContext);

      // Get the runtime function
      Function *runtimeFunc = getRuntimeFunction(M, TEXAS_MALLOC);

      // Get some default values we need later on
      Instruction *tempI = nullptr;
      const Twine &NameStr = "";

      // Get first instruction of main function
      Function *mainFunc = M.getFunction(MAIN);
      Instruction *firstInstOfMain = &(mainFunc->getEntryBlock().front());

      // Go through instructions
      for (auto entry : entries) {

        // Get current inst debug location
        const DebugLoc &instDebugLoc = getDefaultDebugLoc(entry->value);

        // Pointer cast for address
        CastInst *pointerCast = CastInst::CreatePointerCast(
            entry->allocPointer, voidPointerType, NameStr, tempI);
        pointerCast->setDebugLoc(instDebugLoc);

        // Pointer cast for size of the malloc
        CastInst *int64Cast = nullptr;
        if (entry->size != 0) { // size is already known at compile time (e.g., allocainst)
          APInt sizeValue(64, entry->size);
          Constant *sizeConstant =
            Constant::getIntegerValue(int64Type, sizeValue);
          int64Cast = CastInst::CreateZExtOrBitCast(sizeConstant, int64Type,
              NameStr, tempI);
        } else {
          int64Cast = CastInst::CreateZExtOrBitCast(entry->allocSize,
              int64Type, NameStr, tempI);
        }
        int64Cast->setDebugLoc(instDebugLoc);

        // ID
        uint64_t id = entry->id;
        APInt IDValue(64, id);
        Constant *IDConstant =
          Constant::getIntegerValue(int64Type, IDValue);

        // Create call instruction to runtime
        std::vector<Value *> callVals;
        callVals.push_back(IDConstant);
        callVals.push_back(pointerCast);
        callVals.push_back(int64Cast);

        Instruction *insertPoint = nullptr;
        if (Instruction *inst = dyn_cast<Instruction>(entry->value)){ // It's an instruction
          // Safety check for callstack
          Function* currInstFunc = inst->getFunction();
          if (funcToCallstack.count(currInstFunc) == 0){
            errs() << "ERROR: no callstack for function " << currInstFunc->getName() << "\n"; 
            abort();
          }
          Instruction *callstackInst = funcToCallstack[currInstFunc];
          callVals.push_back(callstackInst);
          insertPoint = inst;

        } else if (GlobalVariable *global = dyn_cast<GlobalVariable>(entry->value)) { // It's a global
          if (funcToCallstack.count(mainFunc) == 0){
            errs() << "ERROR: no callstack for function " << mainFunc->getName() << "\n"; 
            abort();
          }
          Instruction *callstackInst = funcToCallstack[mainFunc];
          callVals.push_back(callstackInst);
          insertPoint = firstInstOfMain;

        } else {
          errs() << "ERROR: instruction " << *(entry->value) << " is not and Instruction nor a GlobalVariable. Abort.\n";
          abort();
        }

        ArrayRef<Value *> callArgs = ArrayRef<Value *>(callVals);
        CallInst *callToRuntime =
          CallInst::Create(runtimeFunc, callArgs);
        callToRuntime->setDebugLoc(instDebugLoc);

        std::vector<Instruction *> instsToInsert = {pointerCast, int64Cast, callToRuntime};
        insertInstructionsWithCheck(instsToInsert, insertPoint);

        // At this point we have modified the IR
        modified = true;
      }

      return modified;
    }

    // Create ID map
    void createDebugDataForVector(std::vector<Entry*> entries, std::vector<DebugData> &debugData){
      for (Entry *entry : entries){
        uint64_t ID = debugData.size() + 1; // 0 is a reserved value in the runtime, ID 0 means do not track this.
        entry->id = ID;
        DebugData debugDatum = entry->getDebugData();
        debugData.push_back(debugDatum);
      }

      return;
    }

    void createDebugData(std::vector<std::vector<Entry*>> &allEntries, std::vector<DebugData> &debugData){
      for (auto &entries : allEntries){
        createDebugDataForVector(entries, debugData);
      }

      return;
    }

    void writeDebugData(std::vector<DebugData> &debugData) {
      // Create file in current dir
      std::string fileName = "debugData.dat";
      std::ofstream wf(fileName, std::ios::out | std::ios::binary);
      if (!wf) {
        errs() << "ERROR: cannot open file " << fileName << "\n";
      }

      // Write map into file
      for (auto i = 0; i < debugData.size(); ++i){
        DebugData *elem = &debugData[i];
        wf.write((char*) elem, sizeof(DebugData));
      }

      // Close file
      wf.close();
      if(!wf.good()) {
        errs() << "ERROR: cannot close file " << fileName << "\n";
      }

      return;
    }

    std::unordered_set<Instruction*> mergeSets(std::vector<std::unordered_set<Instruction*>> &sets){
      std::unordered_set<Instruction*> mergeSet;
      for (auto &elem : sets){
        std::merge(elem.begin(), elem.end(), mergeSet.begin(), mergeSet.end(), std::inserter(mergeSet, mergeSet.begin()));
      }

      return mergeSet;
    }


    void printMap(std::unordered_map<LoadStoreInstInfo*, std::unordered_set<ExitBasicBlockInfo*>> &storesToInstrument){
      for (auto elem : storesToInstrument){
        errs() << "STOREINST " << elem.first->inst << *elem.first->inst << "\n";
        for (auto exitBasicBlockInfo : elem.second){
          BasicBlock *exitingBasicBlock = exitBasicBlockInfo->exitingBasicBlock;
          BasicBlock *exitBasicBlock = exitBasicBlockInfo->exitBasicBlock;

          errs() << "EXIT BB " << exitBasicBlock << *exitBasicBlock << "\n\n";
          errs() << "EXITING BB " << exitingBasicBlock << *exitingBasicBlock << "\n";
        }
      }

      return;
    }

    std::unordered_set<Instruction*> getInsts(std::unordered_map<LoadStoreInstInfo*, std::unordered_set<ExitBasicBlockInfo*>> &map){
      std::unordered_set<Instruction*> insts;
      for (auto elem : map){
        Instruction *inst = elem.first->inst;
        insts.insert(inst);
      }

      return insts;
    }

    void addToMap(std::unordered_map<LoadStoreInstInfo*, std::unordered_set<ExitBasicBlockInfo*>> &map, std::unordered_map<LoadStoreInstInfo*, std::unordered_set<ExitBasicBlockInfo*>> &newMap){
      std::unordered_set<Instruction*> insts = getInsts(newMap);
      for (auto elem : map){
        LoadStoreInstInfo *loadStoreInstInfo = elem.first;
        Instruction *inst = loadStoreInstInfo->inst;
        if (insts.count(inst)){
          errs() << "ERROR: store inst " << *inst << " is being instrumented twice. Abort.\n";
          abort();
        }
        for (auto exitBasicBlockInfo : elem.second){
          newMap[loadStoreInstInfo].insert(exitBasicBlockInfo);
        }
      }

      return;
    }

    std::unordered_map<LoadStoreInstInfo*, std::unordered_set<ExitBasicBlockInfo*>> mergeMaps(std::unordered_map<LoadStoreInstInfo*, std::unordered_set<ExitBasicBlockInfo*>> &map1, std::unordered_map<LoadStoreInstInfo*, std::unordered_set<ExitBasicBlockInfo*>> &map2){
      std::unordered_map<LoadStoreInstInfo*, std::unordered_set<ExitBasicBlockInfo*>> newMap;
      addToMap(map1, newMap);
      addToMap(map2, newMap);

      return newMap;
    }

    bool setRuntimeGlobalVariable(Module &M, std::string globalName, bool disable, Instruction *insertPoint){
      bool modified = false;

      GlobalVariable *gv = M.getGlobalVariable(globalName);
      if (gv == nullptr){
        errs() << "ERROR: global variable " << globalName << " does not exist. Abort.\n";
        abort();
      }

      int disableAsInt = ((int) !disable); // needs to be negated
      Constant *value = ConstantInt::getSigned(gv->getValueType(), disableAsInt);
      StoreInst *storeInst = new StoreInst(value, gv, insertPoint);

      // If we are here we modified the bitcode
      modified = true;

      return modified;
    }

    bool setRuntimeGlobalVariables(Module &M){
      bool modified = false;
      LLVMContext &context = M.getContext();
      Type *int64Type = Type::getInt64Ty(context);

      // Get options passed to this pass
      int disableStateInBytes = (disableStateInBytesOpt.getNumOccurrences() > 0) ? 0 : 1;
      int disableInput = (disableInputOpt.getNumOccurrences() > 0) ? 0 : 1;
      int disableOutput = (disableOutputOpt.getNumOccurrences() > 0) ? 0 : 1;
      int disableCloneable = (disableCloneableOpt.getNumOccurrences() > 0) ? 0 : 1;
      int disableTransfer = (disableTransferOpt.getNumOccurrences() > 0) ? 0 : 1;
      int trackCycles = (cycleFindingOpt.getNumOccurrences() > 0) ? 1 : 0; // no need to reverse this
      int disableStateUses = (disableStateUsesOpt.getNumOccurrences() > 0) ? 1 : 0; // No need to reverse this
      int disableCallstack = (disableCallstackOpt.getNumOccurrences() > 0) ? 1 : 0; // No need to reverse this

      // Create call to prologue
      APInt apintDisableStateInBytes(64, disableStateInBytes);
      Constant *constantDisableStateInBytes = Constant::getIntegerValue(int64Type, apintDisableStateInBytes);

      APInt apintDisableInput(64, disableInput);
      Constant *constantDisableInput = Constant::getIntegerValue(int64Type, apintDisableInput);

      APInt apintDisableOutput(64, disableOutput);
      Constant *constantDisableOutput = Constant::getIntegerValue(int64Type, apintDisableOutput);

      APInt apintDisableCloneable(64, disableCloneable);
      Constant *constantDisableCloneable = Constant::getIntegerValue(int64Type, apintDisableCloneable);

      APInt apintDisableTransfer(64, disableTransfer);
      Constant *constantDisableTransfer = Constant::getIntegerValue(int64Type, apintDisableTransfer);

      APInt apintTrackCycles(64, trackCycles);
      Constant *constantTrackCycles = Constant::getIntegerValue(int64Type, apintTrackCycles);

      APInt apintDisableStateUses(64, disableStateUses);
      Constant *constantDisableStateUses = Constant::getIntegerValue(int64Type, apintDisableStateUses);

      APInt apintDisableCallstack(64, disableCallstack);
      Constant *constantDisableCallstack = Constant::getIntegerValue(int64Type, apintDisableCallstack);

      std::vector<Value *> callVals;
      callVals.push_back(constantDisableStateInBytes);
      callVals.push_back(constantDisableInput);
      callVals.push_back(constantDisableOutput);
      callVals.push_back(constantDisableCloneable);
      callVals.push_back(constantDisableTransfer);
      callVals.push_back(constantTrackCycles);
      callVals.push_back(constantDisableStateUses);
      callVals.push_back(constantDisableCallstack);

      Function *main = M.getFunction("main");
      Instruction *firstInstOfMain = &(main->getEntryBlock().front());
      errs() << "FIRST INST OF MAIN " << *firstInstOfMain << "\n";

      Function *prologueFunc = getRuntimeFunction(M, MEMORYTOOL_PROLOGUE);
      ArrayRef<Value *> callArgs = ArrayRef<Value *>(callVals);
      CallInst *prologueCallInst = CallInst::Create(prologueFunc, callArgs, "", firstInstOfMain);

      // If we're here, we have modified the IR
      modified = true;

      return modified;
    }

    std::unordered_set<Instruction*> getExitInstructions(Function *F){
      std::unordered_set<Instruction*> exits;
      for (auto &BB : *F){
        for (auto &I : BB){
          if (CallInst *callInst = dyn_cast<CallInst>(&I)){
            Function *caller = callInst->getCaller();
            if (caller->getName() == "exit"){
              exits.insert(&I);
            }
          } else if (ReturnInst *returnInst = dyn_cast<ReturnInst>(&I)){
            exits.insert(&I);
          }
        }
      }

      return exits;
    }

    bool insertEpilogue(Module &M) {
      bool modified = false;

      Function *main = M.getFunction("main");
      std::unordered_set<Instruction*> exits = getExitInstructions(main);

      Function *epilogueFunc = getRuntimeFunction(M, MEMORYTOOL_EPILOGUE);

      for (Instruction *exitPoint : exits){
        CallInst *callToRuntime = CallInst::Create(epilogueFunc);
        callToRuntime->insertBefore(exitPoint);
      }

      modified = true;

      return modified;
    }

    void printSet(Noelle &noelle, std::unordered_set<Instruction*> &set){
      std::vector<std::pair<uint64_t, Instruction*>> ranked;
      auto hot = noelle.getProfiles();
      for (auto inst : set) {
        auto invocations = hot->getInvocations(inst);
        ranked.push_back(std::make_pair(invocations, inst));
      }

      std::sort(ranked.rbegin(), ranked.rend());

      for (auto &elem : ranked){
        auto hotness = elem.first;
        auto inst = elem.second;
        errs() << "inst " << *inst << " func " << inst->getFunction()->getName() << " hotness " << hotness <<  "\n";
      }

      return; 
    }

    void visitOutgoingEdges(llvm::noelle::CallGraph *callGraph, std::unordered_set<CallGraphFunctionFunctionEdge *> &outgoingEdges, std::unordered_set<Function*> &visited){
      for (auto outgoingEdge : outgoingEdges){
        auto functionNode = outgoingEdge->getCallee();
        Function *function = functionNode->getFunction();
        if (visited.count(function)){ // Already seen this function, skip it
          continue;
        }

        visited.insert(function);

        auto currentFunctionNodeOutgoingEdges = functionNode->getOutgoingEdges();
        visitOutgoingEdges(callGraph, currentFunctionNodeOutgoingEdges, visited);
      }

      return;
    }

    std::unordered_set<Function*> getFunctionsThatCallStartTracking(Module &M, llvm::noelle::CallGraph *callGraph){
      Function *startTrackingFunction = getRuntimeFunction(M, MEMORYTOOL_START_TRACKING);

      // Populate functions to instrument load/store with incoming edges
      std::unordered_set<Function *> functionsThatCallStartTracking;
      auto startTrackingFunctionNode = callGraph->getFunctionNode(startTrackingFunction);
      auto incomingEdges = startTrackingFunctionNode->getIncomingEdges();
      for (auto incomingEdge : incomingEdges){
        Function *caller = incomingEdge->getCaller()->getFunction();
        functionsThatCallStartTracking.insert(caller);
      }

      return functionsThatCallStartTracking;
    }

    std::unordered_set<Function*> getFunctionsToInstrumentLoadStore(Module &M, llvm::noelle::CallGraph *callGraph){
      std::unordered_set<Function *> functionsToInstrumentLoadStore;

      // Populate functions to instrument load/store with incoming edges
      std::unordered_set<Function *> functionsThatCallStartTracking = getFunctionsThatCallStartTracking(M, callGraph);

      // Follow the children of functions that call start tracking, and add them to the set
      std::unordered_set<Function*> visited;
      for (auto child : functionsThatCallStartTracking) {
        auto childNode = callGraph->getFunctionNode(child);
        auto outgoingEdges = childNode->getOutgoingEdges();
        visitOutgoingEdges(callGraph, outgoingEdges, visited);
      }

      // Union of the previous 2 sets
      std::set_union(functionsThatCallStartTracking.begin(), functionsThatCallStartTracking.end(), visited.begin(), visited.end(), std::inserter(functionsToInstrumentLoadStore, functionsToInstrumentLoadStore.begin()));

      return functionsToInstrumentLoadStore;
    }

    std::vector<Entry*> filterLibraryCalls(Noelle &noelle, std::vector<Entry*> &unknowns){
      std::unordered_set<std::string> funcNamesToSkip = {"fprintf", "fwrite", "printf", "fflush", "exit", "strcmp", "atoi", "fopen", "fgetc_unlocked", "fclose", "fread", "puts", "stat", "__iso99_sscanf", "atof", "sprintf", "__iso99_fscanf", "fgets", "perror", "getc", "strlen", "strncmp", "fputc", "ftell", "fseek", "feof", "strtol", "putchar", "__errno_location", "omp_get_max_threads", "omp_set_max_threads", "strerror", "vprintf", "snprintf", "vsnprintf"};

      // Filter library calls
      std::vector<Entry*> filteredUnknowns;
      auto funcManager = noelle.getFunctionsManager();
      for (auto elem : unknowns){
        Function *func = nullptr;
        if (auto inst = dyn_cast<CallInst>(elem->value)){
          func = inst->getCalledFunction();

        } else if (auto inst = dyn_cast<InvokeInst>(elem->value)) {
          func = inst->getCalledFunction();
        }

        if (func == nullptr){
          abort();
        }

        std::string funcName = func->getName();

        if(funcManager->isTheLibraryFunctionPure(func)){
          continue;
        }

        if (funcNamesToSkip.count(funcName)){
          continue;
        }

        errs() << "Function call instrumented with PIN: " << funcName << "\n" ;

        filteredUnknowns.push_back(elem);
      }

      return filteredUnknowns;
    }

    void fixStartTracking(std::vector<Instruction*> &callsToStartTracking){
      for (auto inst : callsToStartTracking){
        ;
      }

      return;
    }

    std::unordered_set<Instruction*> getLeftoverLoadStore(std::unordered_map<Instruction*, Entry*> &instToEntryMap, std::unordered_set<Instruction*> &alreadyInstrumented){
      std::unordered_set<Instruction*> leftover;
      for (auto elem : instToEntryMap) {
        Instruction *inst = elem.first;
        if (alreadyInstrumented.count(inst)){ // This load/store has already been instrumented
          continue;
        }

        leftover.insert(inst);
      }

      return leftover;
    }

    void printStatistics(Module &M, Noelle &noelle, std::unordered_set<Function*> &functionsToInstrumentLoadStore, std::unordered_set<Instruction*> &touchesToSkip, std::vector<ROI*> &rois){
      std::unordered_set<Instruction*> notInstrumented;
      std::unordered_set<Instruction*> instrumented;
      for (auto func : functionsToInstrumentLoadStore){
        for (auto &BB : *func){
          for (auto &I : BB){
            if (touchesToSkip.count(&I)){
              notInstrumented.insert(&I);
              continue;
            }

            if (LoadInst *loadInst = dyn_cast<LoadInst>(&I)){
              instrumented.insert(&I);
            } else if (StoreInst *storeInst = dyn_cast<StoreInst>(&I)){
              instrumented.insert(&I);
            }

          }
        }
      }

      auto hot = noelle.getProfiles();
      uint64_t totalNotInstrumentedHotness = 0;
      for (auto I : notInstrumented){
        auto invocations = hot->getInvocations(I);
        totalNotInstrumentedHotness += invocations;
      }

      uint64_t totalInstrumentedHotness = 0;
      for (auto I : instrumented){
        auto invocations = hot->getInvocations(I);
        totalInstrumentedHotness += invocations;
      }

      for (auto roi : rois){
        Function *funcROIisIn = roi->start->getFunction();
        auto coverage = hot->getDynamicTotalInstructionCoverage(funcROIisIn);
        errs() << "Function " << funcROIisIn->getName() << " has coverage = " << coverage << "\n";
      }

      errs() << "totalNotInstrumentedHotness = " << totalNotInstrumentedHotness << "\n";
      errs() << "totalInstrumentedHotness = " << totalInstrumentedHotness << "\n";
      errs() << "totalInstrumentedHotness/(totalInstrumentedHotness + totalNotInstrumentedHotness) " << (double)totalInstrumentedHotness/((double)totalInstrumentedHotness + (double)totalNotInstrumentedHotness) << "\n";
      errs() << "totalNotInstrumentedHotness/(totalInstrumentedHotness + totalNotInstrumentedHotness) " << (double)totalNotInstrumentedHotness/((double)totalInstrumentedHotness + (double)totalNotInstrumentedHotness) << "\n";


      Function *mainFunc = M.getFunction("main");
      auto totalMainInst = hot->getTotalInstructions(mainFunc);
      for (auto roi : rois){
        errs() << "ROI STATS: ";
        StayConnectedNestedLoopForestNode *roiLoop = getROILoop(noelle, roi->start);
        if (roiLoop == nullptr){
          errs() << " not in a loop\n";
          continue;
        }

        LoopStructure *roiLoopStructure = roiLoop->getLoop();
        auto totalLoopInst = hot->getTotalInstructions(roiLoopStructure);
        auto loopCoverage =  (hot->getDynamicTotalInstructionCoverage(roiLoopStructure) * 100);

        errs() << " hotness " << ((double)totalLoopInst/(double)totalMainInst);
        errs() << " coverage " << loopCoverage;

        /*
           auto loopFunction = LS->getFunction();
           auto entryInst = LS->getEntryInstruction();
           errs() << "Loop:\n" ;
           errs() << "  " << loopFunction->getName() << "\n";
           errs() << "  " << *entryInst << "\n";
           errs() << "  Self  = " << hot->getSelfInstructions(LS) << "\n";
           errs() << "  Total = " << hot->getTotalInstructions(LS) << "\n";

           errs() << "    Number of invocations of the loop = " << hot->getInvocations(LS) << "\n";
           errs() << "    Average number of iterations per invocations = " << hot->getAverageLoopIterationsPerInvocation(LS) << "\n";
           errs() << "    Average number of total instructions per invocations = " << hot->getAverageTotalInstructionsPerInvocation(LS) << "\n";
           errs() << "    Coverage in terms of total instructions = " << (hot->getDynamicTotalInstructionCoverage(LS) * 100) << "%\n";
           */

        errs() << "\n";
      }

      return;
    }


    //#define MEMORYTOOL_DISABLE_PIN
    //#define MEMORYTOOL_DISABLE_LOOP_INVARIANT_STORE_OPT
    //#define MEMORYTOOL_DISABLE_LOOP_INDUCTION_STORE_OPT
    //#define MEMORYTOOL_DISABLE_MOSTLY_LOOP_INVARIANT_STORE_OPT
    //#define MEMORYTOOL_DISABLE_STORE_DFA_OPT
    //#define MEMORYTOOL_DISABLE_ONLY_READ_OPT

    bool runOnModule(Module &M) override {
      bool modified = false;

      // Get ROIs
      ROIManager roiManager(M);
      std::vector<ROI*> rois = roiManager.getROIs(M);
      if (rois.size() == 0){
        errs() << "WARNING: no ROI found. We will not instrument anything. Bye.\n";
        return false;
      }

      // Fetch Noelle
      auto &noelle = getAnalysis<Noelle>();

      auto functionsManager = noelle.getFunctionsManager();
      auto callGraph = functionsManager->getProgramCallGraph();

      // Get functions for which to instrument loads and stores
      std::unordered_set<Function*> functionsToInstrumentLoadStore = getFunctionsToInstrumentLoadStore(M, callGraph);

      // If we do not track uses
      std::unordered_set<Instruction*> touchesToSkip;
      bool enableDFAObjGranularity = (enableDFAObjGranularityOpt.getNumOccurrences() > 0);
#ifndef MEMORYTOOL_DISABLE_STORE_DFA_OPT
      touchesToSkip = getTouchSameObjectInsts(M, noelle, functionsToInstrumentLoadStore, rois, enableDFAObjGranularity);
#endif

      // Add ROI related loads/stores to uses to skip
#ifndef MEMORYTOOL_DISABLE_SKIP_ROIS_LOADS_STORES
      std::unordered_set<Instruction*> roisLoadsStores = roiManager.getROIsLoadsStores();
      std::vector<std::unordered_set<Instruction*>> setsToMergeToSkipPre = {touchesToSkip, roisLoadsStores};
      touchesToSkip = mergeSets(setsToMergeToSkipPre);
#endif


      //printStatistics(M, noelle, functionsToInstrumentLoadStore, touchesToSkip, rois);


      errs() << "NUMBER OF USES TO SKIP = " << touchesToSkip.size() << "\n";
      printSet(noelle, touchesToSkip);

      // Get functions to instrument AllocaInst
      std::unordered_set<Function*> functionsToInstrumentAllocaInst = findFunctionsToInstrument(M);

      // Instruction visitor: visit all instructions of interest and filter them
      MyInstVisitor IV(functionsToInstrumentLoadStore,
          functionsToInstrumentAllocaInst, functionCalls, M,
          touchesToSkip);
      IV.visit(M);

      bool disableStateUsesInstrumentation = (disableStateUsesInstrumentationOpt.getNumOccurrences() > 0);
      // If we disable uses instrumentation we can just clear instToEntryMap, which contains all loads and stores that will be instrumented
      if (disableStateUsesInstrumentation){
        IV.instToEntryMap.clear();
      }

      // Filter library calls
      IV.unknowns = filterLibraryCalls(noelle, IV.unknowns);

      // Visit all globals (can not use a visitor for this)
      getGlobals(M, IV.allocations);

      // Create dictionary id -> file,line,column, dump to a file in text format (for allocations, stores, loads, and call/invoke). Return a map Instruction* -> id.
      std::vector<std::vector<Entry*>> allEntries = {IV.allocations, IV.callocs, IV.reallocs, IV.unknowns, IV.stores, IV.loads};
      std::vector<DebugData> debugData;
      createDebugData(allEntries, debugData);
      writeDebugData(debugData);

      std::unordered_set<Instruction*> instsToSkip;
      // Analyze only read
      std::unordered_set<Instruction*> onlyReadToInstrument;
#ifndef MEMORYTOOL_DISABLE_ONLY_READ_OPT
      onlyReadToInstrument = getOnlyReadLoadsAll(noelle, rois, instsToSkip, IV.instToEntryMap);
#endif
      errs() << "NUMBER OF ONLY READ = " << onlyReadToInstrument.size() << "\n";
      printSet(noelle, onlyReadToInstrument);


      std::vector<std::unordered_set<Instruction*>> setsToMergeToSkip = {instsToSkip, onlyReadToInstrument};
      instsToSkip = mergeSets(setsToMergeToSkip);


      // Get direct state memory operations (loads, stores)
      std::unordered_set<Instruction*> directStateToInstrument;
#ifndef MEMORYTOOL_DISABLE_DIRECT_STATE
      std::vector<DirectStateLoadStore> directStateToInstrumentAll = getDirectStateLoadStoreAll(noelle, rois, instsToSkip, IV.instToEntryMap);
      directStateToInstrument = getDirectStateLoadStoreInsts(directStateToInstrumentAll);
#endif
      errs() << "NUMBER OF DIRECT STATE = " << directStateToInstrument.size() << "\n";
      printSet(noelle, directStateToInstrument);


      setsToMergeToSkip = {instsToSkip, directStateToInstrument};
      instsToSkip = mergeSets(setsToMergeToSkip);


      // TODO: to remove
      std::unordered_set<Instruction*> leftover = getLeftoverLoadStore(IV.instToEntryMap, instsToSkip);
      errs() << "NUMBER OF LEFTOVER LOAD/STORE = " << leftover.size() << "\n";
      printSet(noelle, leftover);


      // Analyze invariants
      std::unordered_set<Instruction*> invariantsToInstrument;
#ifndef MEMORYTOOL_DISABLE_LOOP_INVARIANT_STORE_OPT
      invariantsToInstrument = getLoopInvariants(M, noelle, instsToSkip, IV.instToEntryMap);
#endif
      errs() << "NUMBER OF INVARIANTS = " << invariantsToInstrument.size() << "\n";

      setsToMergeToSkip = {instsToSkip, invariantsToInstrument};
      instsToSkip = mergeSets(setsToMergeToSkip);

      // Analyze mostly invariants
      std::unordered_set<Instruction*> mostlyInvariantsToInstrument;
#ifndef MEMORYTOOL_DISABLE_MOSTLY_LOOP_INVARIANT_STORE_OPT
      mostlyInvariantsToInstrument = getLoopMostlyInvariants(noelle, instsToSkip, IV.instToEntryMap);
#endif
      errs() << "NUMBER OF MOSTLY INVARIANTS = " << mostlyInvariantsToInstrument.size() << "\n";

      setsToMergeToSkip = {instsToSkip, mostlyInvariantsToInstrument};
      instsToSkip = mergeSets(setsToMergeToSkip);

      // Analyze induction variables related load/store
      std::unordered_set<Instruction*> inductionsToInstrument;
#ifndef MEMORYTOOL_DISABLE_LOOP_INDUCTION_STORE_OPT
      if (enableDFAObjGranularity){
        inductionsToInstrument = getLoopInductions(noelle, instsToSkip, IV.instToEntryMap);
      }
#endif
      errs() << "NUMBER OF INDUCTIONS = " << inductionsToInstrument.size() << "\n";

      setsToMergeToSkip = {instsToSkip, inductionsToInstrument};
      instsToSkip = mergeSets(setsToMergeToSkip);

      // Analyze induction variables related load/store
      std::unordered_set<Instruction*> baseAddressMostlyInvariantsToInstrument;
#ifndef MEMORYTOOL_DISABLE_BASE_ADDRESS_MOSTLY_INVARIANT_STORE_OPT
      if (enableDFAObjGranularity){
        baseAddressMostlyInvariantsToInstrument = getBaseAddressMostlyInvariants(instsToSkip, IV.instToEntryMap);
      }
#endif
      errs() << "NUMBER OF BASE ADDRESS MOSTLY INVARIANTS = " << baseAddressMostlyInvariantsToInstrument.size() << "\n";

      setsToMergeToSkip = {instsToSkip, baseAddressMostlyInvariantsToInstrument};
      instsToSkip = mergeSets(setsToMergeToSkip);

      errs() << "INSTS TO SKIP = " << instsToSkip.size() << "\n";
      errs() << "INST TO ENTRY MAP = " << IV.instToEntryMap.size() << "\n";

      std::unordered_set<Function*> uniqueCallstackFunctions = getUniqueCallstackFunctions(noelle);
      errs() << "NUMBER OF UNIQUE CALLSTACKS = " << uniqueCallstackFunctions.size() << "\n";


      // WE START MODIFYING THE BITCODE HERE

      bool disableAllInstrumentation = (disableAllInstrumentationOpt.getNumOccurrences() > 0);

      // Insert runtime functions into module
      modified |= insertRuntimeFunctions(M);
      assert(!verifyModule(M, &errs()));

      if (!disableAllInstrumentation){
        // Insert calls to generate callstack at the beginning of every function
        std::map<Function*, Instruction*> funcToCallstack = callstackInstrumentation(M, uniqueCallstackFunctions);
        modified |= (funcToCallstack.size() > 0); // If there is at least one entry in this map, then we have modified the bitcode

        // Instrument module ir code
        // State allocations
        assert(!verifyModule(M, &errs()));
        modified |= instrumentAllocations(M, IV.allocations, funcToCallstack);
        assert(!verifyModule(M, &errs()));
        modified |= instrumentCallocs(M, IV.callocs, funcToCallstack);
        assert(!verifyModule(M, &errs()));
        modified |= instrumentReallocs(M, IV.reallocs, funcToCallstack);
        assert(!verifyModule(M, &errs()));

        bool cycleFinding = (cycleFindingOpt.getNumOccurrences() > 0);

#ifndef MEMORYTOOL_DISABLE_PIN
        if (!cycleFinding) {
          // Pin instrumentation
          modified |= instrumentUnkowns(M, IV.unknowns, funcToCallstack);
          assert(!verifyModule(M, &errs()));
        }
#endif

        // State modifications

#ifndef MEMORYTOOL_DISABLE_ONLY_READ_OPT
        if (!cycleFinding) {
          modified |= instrumentOnlyReads(M, funcToCallstack, IV.instToEntryMap, onlyReadToInstrument);
          assert(!verifyModule(M, &errs()));
        }
#endif

#ifndef MEMORYTOOL_DISABLE_DIRECT_STATE
        if (!cycleFinding) {
          modified |= instrumentDirectStateLoadStore(M, funcToCallstack, IV.instToEntryMap, directStateToInstrumentAll);
          assert(!verifyModule(M, &errs()));
        }
#endif


        // Instrument invariants loads and stores
#ifndef MEMORYTOOL_DISABLE_LOOP_INVARIANT_STORE_OPT
        //modified |= instrumentLoopInvariants(M, funcToCallstack, IV.instToEntryMap, invariantsToInstrument);
        //assert(!verifyModule(M, &errs()));
#endif

        // Instrument mostly invariants loads and stores
#ifndef MEMORYTOOL_DISABLE_MOSTLY_LOOP_INVARIANT_STORE_OPT
        //modified |= instrumentLoopMostlyInvariants(M, funcToCallstack, IV.instToEntryMap, mostlyInvariantsToInstrument);
        //assert(!verifyModule(M, &errs()));
#endif

        // Instrument inductions loads and stores same as invariants
#ifndef MEMORYTOOL_DISABLE_LOOP_INDUCTION_STORE_OPT
        //if (enableDFAObjGranularity){
        //  modified |= instrumentLoopInvariants(M, funcToCallstack, IV.instToEntryMap, inductionsToInstrument);
        //errs() << "MODULE\n" << M << "\n";
        //  assert(!verifyModule(M, &errs()));
        //}
#endif

        // Instrument base address mostly invariant loads and stores same as mostly invariants
#ifndef MEMORYTOOL_DISABLE_BASE_ADDRESS_MOSTLY_INVARIANT_STORE_OPT
        //if (enableDFAObjGranularity){
        //  modified |= instrumentLoopMostlyInvariants(M, funcToCallstack, IV.instToEntryMap, baseAddressMostlyInvariantsToInstrument);
        //errs() << "MODULE\n" << M << "\n";
        //  assert(!verifyModule(M, &errs()));
        //}
#endif

        // Instrument the rest of the loads and stores which have not been instrumented yet
        //modified |= instrumentLoadsStores(M, funcToCallstack, IV.instToEntryMap, instsToSkip);
        if (!cycleFinding) {
          modified |= instrumentLoadsStoresRegular(M, funcToCallstack, IV.instToEntryMap, instsToSkip);
          //modified |= instrumentLoadsStoresToRemove(M, funcToCallstack, IV.instToEntryMap, instsToSkip);
          assert(!verifyModule(M, &errs()));
        }

        //State escapes
        if (cycleFinding){
          modified |= instrumentEscapes(M, IV.escapes);
          assert(!verifyModule(M, &errs()));
        }

        // State deallocations
        errs() << "ERASE DEALLOCS " << cycleFinding << "\n";
        if (cycleFinding){
          modified |= eraseDeallocs(IV.deallocs);
          assert(!verifyModule(M, &errs()));
        }

        if (!cycleFinding) {
          modified |= instrumentDeallocs(M, IV.deallocs);
          assert(!verifyModule(M, &errs()));
          modified |= instrumentReturns(M, IV.returns, IV.functionAllocaMap);
          assert(!verifyModule(M, &errs()));
          modified |= instrumentInvokes(M, IV.invokes, IV.functionAllocaMap);
          assert(!verifyModule(M, &errs()));
        }

      } // end of !disableAllInstrumentation

      // Add calls to removeCallstack()
      //modified |= insertRmCallstackInstrumentation(M, IV.returns, IV.invokes);
      //assert(!verifyModule(M, &errs()));

      // Set runtime globals to enable/disable runtime features (e.g., transfer state)
      // In other words, insert prologue function call
      modified |= setRuntimeGlobalVariables(M);
      assert(!verifyModule(M, &errs()));

      // Insert epilogue to print state
      modified |= insertEpilogue(M);
      assert(!verifyModule(M, &errs()));

      // Fix function name and line number of MEMORYTOOL_START_TRACKING
      //fixStartTracking(IV.callsToStartTracking);


      return modified;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      // The following does not work, but this pass assumes -break-crit-edges
      // AU.addRequired<BreakCriticalEdgesPass>();
      AU.addRequired<Noelle>();

      return;
    }

  }; // end of struct CAT

} // end of anonymous namespace

char CAT::ID = 0;
static RegisterPass<CAT> X("CAT", "CAT pass");

// Next there is code to register your pass to "clang"
static CAT *_PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
    [](const PassManagerBuilder &,
      legacy::PassManagerBase &PM) {
    if (!_PassMaker) {
    PM.add(_PassMaker = new CAT());
    }
    }); // ** for -Ox
static RegisterStandardPasses
_RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
    [](const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    if (!_PassMaker) {
    PM.add(_PassMaker = new CAT());
    }
    }); // ** for -O0
