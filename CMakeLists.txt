# Project
cmake_minimum_required(VERSION 3.4.3)
project(CAT)

# Programming languages to use
enable_language(C CXX)

# Find and link with LLVM
find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
add_definitions(
-D__STDC_LIMIT_MACROS
-D__STDC_CONSTANT_MACROS
)

option(MEMORYTOOL_DISABLE_ONLY_READ_OPT "Disable optimization that instruments only read memory location only when they change" OFF)
if (MEMORYTOOL_DISABLE_ONLY_READ_OPT)
  add_definitions(-DMEMORYTOOL_DISABLE_ONLY_READ_OPT)
endif()

option(MEMORYTOOL_DISABLE_BASE_ADDRESS_MOSTLY_INVARIANT_STORE_OPT "Disable loop base address mostly invariant stores" OFF)
if (MEMORYTOOL_DISABLE_BASE_ADDRESS_MOSTLY_INVARIANT_STORE_OPT)
  add_definitions(-DMEMORYTOOL_DISABLE_BASE_ADDRESS_MOSTLY_INVARIANT_STORE_OPT)
endif()

option(MEMORYTOOL_DISABLE_LOOP_INVARIANT_STORE_OPT "Disable loop invariant stores" OFF)
if (MEMORYTOOL_DISABLE_LOOP_INVARIANT_STORE_OPT)
  add_definitions(-DMEMORYTOOL_DISABLE_LOOP_INVARIANT_STORE_OPT)
endif()

option(MEMORYTOOL_DISABLE_MOSTLY_LOOP_INVARIANT_STORE_OPT "Disable mostly loop invariant stores" OFF)
if (MEMORYTOOL_DISABLE_MOSTLY_LOOP_INVARIANT_STORE_OPT)
  add_definitions(-DMEMORYTOOL_DISABLE_MOSTLY_LOOP_INVARIANT_STORE_OPT)
endif()

option(MEMORYTOOL_DISABLE_STORE_DFA_OPT "Disable store DFA" OFF)
if (MEMORYTOOL_DISABLE_STORE_DFA_OPT)
  add_definitions(-DMEMORYTOOL_DISABLE_STORE_DFA_OPT)
endif()

option(MEMORYTOOL_DISABLE_LOOP_INDUCTION_STORE_OPT "Disable loop induction stores" OFF)
if (MEMORYTOOL_DISABLE_LOOP_INDUCTION_STORE_OPT)
  add_definitions(-DMEMORYTOOL_DISABLE_LOOP_INDUCTION_STORE_OPT)
endif()

option(MEMORYTOOL_DISABLE_PIN "Disable CARMOT Pin tool" OFF)
if (MEMORYTOOL_DISABLE_PIN)
  add_definitions(-DMEMORYTOOL_DISABLE_PIN)
endif()

option(MEMORYTOOL_DISABLE_DIRECT_STATE "Disable CARMOT direct state optimization to set a specific FSA state for a chunk of memory" OFF)
if (MEMORYTOOL_DISABLE_DIRECT_STATE)
  add_definitions(-DMEMORYTOOL_DISABLE_DIRECT_STATE)
endif()

option(MEMORYTOOL_DISABLE_SKIP_ROIS_LOADS_STORES "Disable CARMOT ability to ignore loads and stores related to stateID of an ROI" OFF)
if (MEMORYTOOL_DISABLE_SKIP_ROIS_LOADS_STORES)
  add_definitions(-DMEMORYTOOL_DISABLE_SKIP_ROIS_LOADS_STORES)
endif()


set(REPO_PATH $ENV{REPO_PATH})

SET(CMAKE_CXX_FLAGS "-I${REPO_PATH}/src/include -I${REPO_PATH}/inst/include")
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

include_directories(${LLVM_INCLUDE_DIRS} ${REPO_PATH}/tools/noelle/install/include ${REPO_PATH}/tools/noelle/install/include/svf)
link_directories(${LLVM_LIBRARY_DIRS})
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Prepare the pass to be included in the source tree
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# Pass
add_subdirectory(src)

# Install
install(PROGRAMS bin/memorytool_pass DESTINATION bin)
