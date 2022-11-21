# Sources
#set(CATPassSrc 
#  CatPass.cpp
#  Utils.cpp
#  ROI.cpp
#  ROIManager.cpp
#  AvoidLocalVariableInstrumentation.cpp
#  ClusterCallstackInstrumentation.cpp
#  MostlyInvariant.cpp
#  BaseAddressMostlyInvariant.cpp
#  InstrumentLoopInvariantStoreAddressOnce.cpp
#  Induction.cpp
#  LoadStoreInstrumentation.cpp
#  StoreLoopInstrumentation.cpp
#  TouchSameObject.cpp
#  Invariant.cpp
#)


file(GLOB CATPassSrc "*.cpp")
set(CloneFuncPassSrc
  ./srcCloneFunc/MetadataInstruction.cpp
  ./srcCloneFunc/TagROIs.cpp
  )


# Compilation flags
set_source_files_properties(${CATPassSrc} ${CloneFuncPassSrc} PROPERTIES COMPILE_FLAGS " -std=c++17 -fPIC")

# Name of the LLVM pass
set(CATPassName "memorytool_pass")

# configure LLVM 
find_package(LLVM REQUIRED CONFIG)

set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)

message(STATUS "LLVM_DIR IS ${LLVM_CMAKE_DIR}.")

include_directories(${LLVM_INCLUDE_DIRS})


# Declare the LLVM pass to compile
add_llvm_library(${CATPassName} MODULE ${CATPassSrc} ${CloneFuncPassSrc})
#add_llvm_loadable_module(${CATPassName} ${CATPassSrc})
