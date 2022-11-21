all:
	clang++ -emit-llvm -c cycleTest1.cpp
	opt -load ~/texas/src/compiler_passes/allocationTracking/build/TexasAlloc.so -TexasAlloc ./cycleTest1.bc -o cycleTest1.bc
	opt -load ~/texas/src/compiler_passes/escapeTracking/build/TexasEscape.so -TexasEscape ./cycleTest1.bc -o cycleTest1.bc
	llvm-link ~/texas/src/runtime/texas.bc ./cycleTest1.bc -o ./cycleTest1.bc
	clang++ ./cycleTest1.bc -o cycleTest1

clean: 
	rm -f *.o *.bc *.ll cycleTest1
