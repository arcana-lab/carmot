CXXFLAGS=-O0 -g -Xclang -disable-O0-optnone -std=c++17 -pthreads -rdynamic -fno-omit-frame-pointer -mcmodel=large
TBB_LIB=../../../include/runtime/oneTBB/build/linux_intel64_gcc_cc8.3.1_libc2.28_kernel4.18.0_release
LIBS=-lstdc++ -lc -L../../../include -lruntime -L$(TBB_LIB) -ltbb -lboost_serialization
PROGRAM_LIBS="-L. -llibrary"

all: library target_program

library:
	clang++ $(CXXFLAGS) -fPIC -c library.cpp
	clang++ $(CXXFLAGS) -fPIC -shared library.o -o liblibrary.so
	llvm-ar -rv liblibrary.a library.o

target_program:
	clang++ $(CXXFLAGS) -emit-llvm -c test.cpp -o test.bc
	llvm-dis test.bc
	clang++ $(CXXFLAGS) test.bc -o test_orig $(LIBS) -L. -llibrary
	PROGRAM_LIBS="-L. -llibrary" ../../../utils/memorytool-all test.bc

run:
	memorytool-run ./test

run_pin:
	pin -t ${PWD}/../../../utils/MemTrace/obj-intel64/memtrace.so -- ./test

run_pin_orig:
	pin -t ${PWD}/../../../utils/MemTrace/obj-intel64/memtrace.so -- ./test_orig

gdb:
	gdb --args ./test

gdb_pass:
	gdb --args opt -break-crit-edges -load ../../inst/lib/memorytool.so -CAT ./test_orig.bc

clean:
	rm -f ./*.a ./*.symbol ./*.bc ./*.ll ./*.o ./*.so ./*.log ./*.json ./*.oracle ./test_orig ./test
