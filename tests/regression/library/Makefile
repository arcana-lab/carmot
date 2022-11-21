CXXFLAGS=-O0 -g -std=c++17

N=10
S=1
W=0

all: library target_program

library:
	clang++ $(CXXFLAGS) -fPIC -c library.cpp -O2
	clang++ -shared library.o -o liblibrary.so
#	llvm-ar -rv liblibrary.a library.o

target_program:
	clang++ $(CXXFLAGS) -emit-llvm -c test.cpp -o test.bc -fno-exceptions
	clang++ $(CXXFLAGS) test.bc -o test_orig -lstdc++ -lc -L. -llibrary -L../../include -lruntime -fno-exceptions
	
	../../utils/memorytool test.bc -L. -llibrary

run:
	./test

run_pin:
	pin -t /home/mjw6704/memtool/memorytool/utils/MemTrace/obj-intel64/memtrace.so -- ./test $(N) $(S) $(W)

run_pin_orig:
	pin -t /home/mjw6704/memtool/memorytool/utils/MemTrace/obj-intel64/memtrace.so -- ./test_orig

gdb:
	gdb --args ./test

gdb_pass:
	gdb --args opt -break-cirt-edges -load ../../inst/lib/memorytool.so -CAT ./test_orig.bc

clean:
	rm -f ./*.a ./*.symbol ./*.bc ./*.ll ./*.o ./*.json ./*.oracle ./test_orig ./test
