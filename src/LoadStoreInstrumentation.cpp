#include "./include/LoadStoreInstrumentation.hpp"

LoadStoreInstrumentation* getLoadStoreInstrumentation(Module &M, Entry *entry, std::map<Function*, Instruction*> &funcToCallstack){
  // Get the llvm ir data type we need
  LLVMContext &TheContext = M.getContext();
  Type *voidPointerType = Type::getInt8PtrTy(TheContext, 0);
  Type *int64Type = Type::getInt64Ty(TheContext);
  Value *value = entry->value;
  Function *runtimeFunc = nullptr;
  Value *allocPointer = nullptr; // We need this because of BaseAddressInvariant, which changes the pointer to the base address of the GEP
  if (StoreInst *storeInst = dyn_cast<StoreInst>(value)){
    runtimeFunc = getRuntimeFunction(M, TEXAS_ADD_WITH_INFO);
    allocPointer = storeInst->getPointerOperand();
  } else if (LoadInst *loadInst = dyn_cast<LoadInst>(value)){
    runtimeFunc = getRuntimeFunction(M, TEXAS_ADD_WITH_INFO_LOAD);
    allocPointer = loadInst->getPointerOperand();
  } 

  // Get current inst debug location
  const DebugLoc &instDebugLoc = getDefaultDebugLoc(entry->value);

  // Create store pointer to pass as an arg to the runtime
  CastInst *pointerCast = CastInst::CreatePointerCast(allocPointer, voidPointerType);
  pointerCast->setDebugLoc(instDebugLoc);

  // ID
  uint64_t id = entry->id;
  APInt IDValue(64, id);
  Constant *IDConstant =
    Constant::getIntegerValue(int64Type, IDValue);

  // Create call instruction to runtime
  std::vector<Value *> callVals;
  callVals.push_back(IDConstant);
  callVals.push_back(pointerCast);
  
  // Check if we are instrumenting a store
  if (StoreInst *storeInst = dyn_cast<StoreInst>(entry->value)){ // It's an instruction
    // Total size in bytes
    uint64_t size = entry->size;
    APInt sizeValue(64, size);
    Constant *sizeConstant =
      Constant::getIntegerValue(int64Type, sizeValue);
    callVals.push_back(sizeConstant);

    // Element size in bytes
    uint64_t sizeElement = entry->sizeElement;
    APInt sizeElementValue(64, sizeElement);
    Constant *sizeElementConstant =
      Constant::getIntegerValue(int64Type, sizeElementValue);
    callVals.push_back(sizeElementConstant);
  }

  if (Instruction *inst = dyn_cast<Instruction>(entry->value)){ // It's an instruction
    // Safety check for callstack
    Function* currInstFunc = inst->getFunction();
    if (funcToCallstack.count(currInstFunc) == 0){
      errs() << "ERROR: no callstack for function " << currInstFunc->getName() << "\n"; 
      abort();
    }
    Instruction *callstackInst = funcToCallstack[currInstFunc];
    callVals.push_back(callstackInst);

  } else {
    errs() << "ERROR: instruction " << *(entry->value) << " is not a Store Instruction. Abort.\n";
    abort();
  }

  ArrayRef<Value *> callArgs = ArrayRef<Value *>(callVals);
  CallInst *addToState =
    CallInst::Create(runtimeFunc, callArgs);
  addToState->setDebugLoc(instDebugLoc);

  LoadStoreInstrumentation *loadStoreInstrumentation = new LoadStoreInstrumentation(pointerCast, addToState);

  return loadStoreInstrumentation;
}

