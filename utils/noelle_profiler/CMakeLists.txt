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

set(REPO_PATH $ENV{REPO_PATH})

SET(CMAKE_CXX_FLAGS "-I${REPO_PATH}/src/include -I${REPO_PATH}/inst/include")
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

include_directories(${LLVM_INCLUDE_DIRS} ${REPO_PATH}/../../tools/noelle/install/include ${REPO_PATH}/../../tools/noelle/install/include/svf ${REPO_PATH}/../../src/include)
link_directories(${LLVM_LIBRARY_DIRS})
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "REPO_PATH is: ${REPO_PATH}")

# Prepare the pass to be included in the source tree
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# Pass
add_subdirectory(src)

# Install
install(PROGRAMS bin/noelle_profiler DESTINATION bin)
