CXXFLAGS=-O0 -g -std=c++17 -pthreads -rdynamic -fno-omit-frame-pointer -mcmodel=large
TBB_LIB=../../../include/runtime/oneTBB/build/linux_intel64_gcc_cc8.3.1_libc2.28_kernel4.18.0_release
LIBS=-lstdc++ -lc -L../../../include -lruntime -L$(TBB_LIB) -ltbb -lboost_serialization -lboost_system


all: memorytool-all

compile:
	clang++ $(CXXFLAGS) -emit-llvm -c test.cpp -o test.bc
	llvm-dis test.bc
	clang++ $(CXXFLAGS) -pthreads -v test.bc -o test_orig $(LIBS) 

memorytool-all: compile
	$@ test.bc

memorytool-referencecycles: compile
	$@ test.bc

memorytool-stateprivatization: compile
	$@ test.bc

memorytool-criticalsection: compile
	$@ test.bc

memorytool-accelerator: compile
	$@ test.bc

state:
	prettyPrint test

time:
	/usr/bin/time ./test

time_orig:
	/usr/bin/time ./test_orig

massif:
	rm -f massif-output.txt
	valgrind --tool=massif --massif-out-file=massif-output.txt ./test
	ms_print massif-output.txt

massif_orig:
	rm -f massif-output-orig.txt
	valgrind --tool=massif --massif-out-file=massif-output-orig.txt ./test_orig
	ms_print massif-output-orig.txt

valgrind:
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./test

valgrind_orig:
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./test_orig

run:
	memorytool-run ./test `cat memorytool_train_inputs.txt`

run_orig:
	./test_orig

perf:
	perf stat ./test

perf_orig:
	perf stat ./test_orig

gdb:
	gdb --args ./test

gdb_pass:
	gdb --args opt -break-crit-edges -load ../../../inst/lib/memorytool.so -CAT ./test_orig.bc

testPdg: compile
	#noelle-norm test.bc -o testNormalized.bc
	#llvm-dis testNormalized.bc
	./../../../tools/noelle/examples/passes/testPdg/bin/cat-c afterPromoteAllocasROI2.bc -disable-output

clone: compile
	cp test.bc afterCloneFunctionROI.bc
	../../../utils/clone_function_roi/bin/clone_function_roi -emit-llvm -c afterCloneFunctionROI.bc
	llvm-dis afterCloneFunctionROI.bc



clean:
	rm -f ./binaryWithProfile* ./*.profraw ./*.dat ./*.symbol ./*.bc ./*.ll ./*.so ./*.a ./*.o ./*.json ./*.oracle ./test_orig ./test

.PHONY: compile valgrind valgrind_orig run run_orig perf perf_orig gdb gdb_pass clean memorytool-referencecycles memorytool-stateprivatization memorytool-accelerator memorytool-criticalsection memorytool-all
