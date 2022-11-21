# TEXAS: Tool for Allocating And X'forming Allocation State

The TEXAS tool is a research runtime designed to provide information about memory behavior of programs.

## The src/compiler_passes folder 
### Provides compiler passes that inject calls to the runtime to perform the following functions:

allocationTracking: Tracks all allocations made by a program. This includes dynamic allocations, globals, .bss segments, and the stack

escapeTracking: Tracks all pointers to allocations not stored within the initial pointer

stateTracking: Tracks the use of allocations during program execution

manufactureLocality: Transforms program to utilize stateTracking in order to optimize program locality

## The src/runtime folder
### Provides the source code for the TEXAS runtime. This can be compiled as a seperate object file or as LLVM bitcode.

This work is a part of the interweaving project of Northwestern Univeristy (interweaving.org).

## Published Works:

[CARAT: A Case for Virtual Memory via Compiler- And Runtime-based Address Translation](https://users.cs.northwestern.edu/~simonec/files/Research/papers/MODERN_PLDI_2020.pdf) - Brian Suchy, Simone Campanoni, Nikos Hardavellas, and Peter Dinda
