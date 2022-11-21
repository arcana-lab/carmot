all: state 

state: virgil texas.bc

state_no_opt: virgil no_opt.bc

texas_move: texas_move.bc

CFLAGS= -O3 -Wextra -Wall -g -march=native
TBBFLAGS= -I/opt/intel/tbb/include/ 
CXXFLAGS= -O3 -Wextra -I./virgil/include -Wall -std=c++17 -g -march=native -mcmodel=large $(TBBFLAGS) # NO DEBUG PRINTS
#CXXFLAGS= -DDEBUG_OUTPUT -O3 -Wextra -I./virgil/include -Wall -std=c++17 -g -march=native -mcmodel=large $(TBBFLAGS)

#CXXFLAGS= -O3 -pthreads -Wextra -I./virgil/include -Wall -std=c++17 -g -march=native -mcmodel=large $(TBBFLAGS)

Allocation.bc: Allocation.cpp
	clang++ -emit-llvm $(CXXFLAGS) -c Allocation.cpp -o Allocation.bc

AllocationTable.bc: AllocationTable.cpp
	clang++ -emit-llvm $(CXXFLAGS) -c AllocationTable.cpp -o AllocationTable.bc

State.bc: State.cpp
	clang++ -emit-llvm $(CXXFLAGS) -c State.cpp -o State.bc

Shadow.bc: Shadow.cpp
	clang++ -emit-llvm $(CXXFLAGS) -c Shadow.cpp -o Shadow.bc	 

HelperFunctions.bc: HelperFunctions.cpp
	clang++ -emit-llvm $(CXXFLAGS) -c HelperFunctions.cpp -o HelperFunctions.bc	 

texas.bc: texas.cpp State.bc Allocation.bc AllocationTable.bc Shadow.bc HelperFunctions.bc
	clang++ -emit-llvm $(CXXFLAGS) -c texas.cpp -o texas.bc	 
	llvm-link AllocationTable.bc texas.bc -o texas.bc
	llvm-link Allocation.bc texas.bc -o texas.bc
	llvm-link State.bc texas.bc -o texas.bc
	llvm-link Shadow.bc texas.bc -o texas.bc
	llvm-link HelperFunctions.bc texas.bc -o texas.bc

no_opt.bc: texas.cpp State.bc Allocation.bc AllocationTable.bc Shadow.bc HelperFunctions.bc
	clang++ -DMEMORYTOOL_DISABLE_TEXAS_OPT -emit-llvm $(CXXFLAGS) -c texas.cpp -o texas.bc	 
	llvm-link AllocationTable.bc texas.bc -o texas.bc
	llvm-link Allocation.bc texas.bc -o texas.bc
	llvm-link State.bc texas.bc -o texas.bc
	llvm-link Shadow.bc texas.bc -o texas.bc
	llvm-link HelperFunctions.bc texas.bc -o texas.bc


virgil:
	if test -d ./virgil ; then cd ./virgil ; else git clone https://github.com/arcana-lab/virgil.git virgil ; cd virgil ; git checkout 1.0.0 ; fi
	#if test -d ./oneTBB ; then cd ./oneTBB ; git pull ; else git clone https://github.com/oneapi-src/oneTBB.git; cd ./oneTBB; make -j ; fi

texas_move.bc: user_patcher.c patching.cpp runtime_tables.cpp State.cpp 
	clang   -emit-llvm $(CFLAGS)   -c user_patcher.c -o user_patching.bc
	clang++ -emit-llvm $(CXXFLAGS) -c patching.cpp -o patching.bc
	clang++ -emit-llvm $(CXXFLAGS) -c runtime_tables.cpp -o texas.bc	 
	llvm-link texas.bc patching.bc user_patching.bc -o texas.bc
	clang++ -emit-llvm $(CXXFLAGS) -c state_tracking.cpp
	llvm-link texas.bc state_tracking.bc -o texas.bc 

clean: 
	rm -f *.o *.bc *.ll 
	rm -rf ./tlx
	rm -rf virgil

.PHONY: clean virgil oneTBB
