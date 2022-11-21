all:
	clang++ -emit-llvm -c stateTest1.cpp
	opt -load ~/texas/src/compiler_passes/allocationTracking/build/TexasAlloc.so -TexasAlloc ./stateTest1.bc -o stateTest1.bc
	opt -load ~/texas/src/compiler_passes/escapeTracking/build/TexasEscape.so -TexasEscape ./stateTest1.bc -o stateTest1.bc
	opt -load ~/texas/src/compiler_passes/stateTracking/build/StateTracking.so -TexasState ./stateTest1.bc -o stateTest1.bc
	llvm-link ~/texas/src/runtime/texas.bc ./stateTest1.bc -o ./stateTest1.bc
	clang++ ./stateTest1.bc -o stateTest1

clean: 
	rm -f *.o *.bc *.ll *.dot *.json stateTest1
