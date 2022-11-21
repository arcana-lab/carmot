all:
	clang++ -emit-llvm -c cycleTest2.cpp
	opt -load ~/texas/src/compiler_passes/allocationTracking/build/TexasAlloc.so -TexasAlloc ./cycleTest2.bc -o cycleTest2.bc
	opt -load ~/texas/src/compiler_passes/escapeTracking/build/TexasEscape.so -TexasEscape ./cycleTest2.bc -o cycleTest2.bc
	llvm-link ~/texas/src/runtime/texas.bc ./cycleTest2.bc -o ./cycleTest2.bc
	clang++ ./cycleTest2.bc -o cycleTest2

clean: 
	rm -f *.o *.bc *.ll cycleTest2
