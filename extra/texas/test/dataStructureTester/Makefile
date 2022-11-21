all: dataStructureTester.cpp btree.hpp linked_list.hpp splay-tree.c 
	clang++ -emit-llvm -g -std=c++17 -c dataStructureTester.cpp -o dataStructureTester.bc
	clang++ -emit-llvm -g -std=c++17 -c splay-tree.c -o splay-tree.bc
	llvm-link splay-tree.bc dataStructureTester.bc -o dataStructureTester.bc
	gclang++ -g -c dataStructureTester.bc -o dataStructureTester.o
	~/memorytool/memorytool/utils/objcopy.sh TEXAS_ dataStructureTester.o
	gclang++ -g dataStructureTester.o -o dataStructureTester
	get-bc dataStructureTester
	opt -load ~/texas/src/compiler_passes/stateTracking/build/StateTracking.so -TexasState ./dataStructureTester.bc -o dataStructureTester.bc	
	opt -load ~/texas/src/compiler_passes/escapeTracking/build/TexasEscape.so -TexasEscape ./dataStructureTester.bc -o dataStructureTester.bc
	opt -load ~/texas/src/compiler_passes/allocationTracking/build/TexasAlloc.so -TexasAlloc ./dataStructureTester.bc -o dataStructureTester.bc
	cp ~/texas/src/runtime/texas.bc .
	clang++ -g dataStructureTester.bc texas.bc -o dataStructureTesterInst

clean:
	rm -f *.o *.bc dataStructureTester dataStructureTesterInst 



