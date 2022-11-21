all: libsplay-tree.a run-splay



libsplay-tree.a: splay-tree.o
	ar ruv libsplay-tree.a splay-tree.o

splay-tree.o: splay-tree.c splay-tree.h
	$(CC) -Wall -O2 -c splay-tree.c

run-splay: run-splay.c splay-tree.c splay-tree.h
	cd ../../src/runtime ; make clean; make manLoc;
	cp ../../src/runtime/manLoc.bc .
	clang -emit-llvm -c run-splay.c
	clang -emit-llvm -c splay-tree.c
	llvm-link run-splay.bc splay-tree.bc -o run-splay.bc
	noelle-load -load ../../src/compiler_passes/allocationTracking/build/TexasAlloc.so -TexasAlloc run-splay.bc -o run-splay.bc
	noelle-load -load ../../src/compiler_passes/escapeTracking/build/TexasEscape.so -TexasEscape run-splay.bc -o run-splay.bc
	noelle-load -load ../../src/compiler_passes/stateTracking/build/StateTracking.so -StateTracking run-splay.bc -o run-splay.bc
	noelle-load -load ../../src/compiler_passes/manufactureLocality/build/ManufactureLocality.so -ManufactureLocality run-splay.bc -o run-splay.bc
	clang++ -std=c++17 manLoc.bc run-splay.bc -o run-splay

clean:
	rm -f splay-tree.o libsplay-tree.a run-splay ./*.dot ./*.packing ./*.bc ./*.ll 



