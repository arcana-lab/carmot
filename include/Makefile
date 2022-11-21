INCLUDE=-I./runtime/tlx -I./runtime/virgil/include
CPPFLAGS=
TBBFLAGS= -I/opt/intel/tbb/include/ -Wl,-rpath,/opt/intel/tbb/lib/intel64_lin/gcc4.7 -L/opt/intel/tbb/lib/intel64_lin/gcc4.7 -ltbb
CXXFLAGS=-O3 -g -std=c++17 -march=native -mcmodel=large -fno-omit-frame-pointer $(TBBFLAGS)
TBB_LIB = ${LIBTBBSO}/libtbb.so
LIBS = -lboost_serialization -lboost_system

all: wrapper_static

wrapper_dynamic: runtime
	clang++ $(CXXFLAGS) -emit-llvm -c wrapper.cpp
	llvm-link ./runtime/texas.bc wrapper.bc -o runtime.bc
	clang++ $(CXXFLAGS) -c runtime.bc
	#llvm-objcopy --prefix-symbols=MEMORYTOOL_ runtime_internal.o
	clang++ $(CXXFLAGS) -shared -lstdc++ runtime.o -o libruntime.so
	#llvm-objcopy --prefix-symbols=MEMORYTOOL_ libruntime.so
	#clang++ $(CXXFLAGS) -emit-llvm -c wrapper.cpp
	#clang++ $(CXXFLAGS) -c wrapper.bc
	#clang++ $(CXXFLAGS) -shared -lstdc++ -lc runtime_internal.o wrapper.o -o libruntime.so

wrapper_static: runtime
	clang++ $(CXXFLAGS) -static -c backtrace_asm.S
	clang++ $(CXXFLAGS) -O0 -static -c pin_interface.cpp
	clang++ $(CXXFLAGS) $(INCLUDE) -static -c utils.cpp
	clang++ $(CXXFLAGS) $(CPPFLAGS) $(INCLUDE) -static -c -emit-llvm wrapper.cpp $(LIBS)
	llvm-dis wrapper.bc
	clang++ $(CXXFLAGS) $(CPPFLAGS) $(INCLUDE) -static -c wrapper.cpp $(LIBS)
	clang++ $(CXXFLAGS) $(INCLUDE) -static -c ./runtime/texas.bc
	llvm-ar -rv libruntime.a texas.o backtrace_asm.o pin_interface.o utils.o wrapper.o $(TBB_LIB)

wrapper_measure_batching_overhead: runtime
	clang++ $(CXXFLAGS) -static -c backtrace_asm.S
	clang++ $(CXXFLAGS) -O0 -static -c pin_interface.cpp
	clang++ $(CXXFLAGS) $(CPPFLAGS) $(INCLUDE) -DMEMORYTOOL_BATCHING_OVERHEAD -static -c -emit-llvm wrapper.cpp $(LIBS)
	llvm-dis wrapper.bc
	clang++ $(CXXFLAGS) $(CPPFLAGS) $(INCLUDE) -DMEMORYTOOL_BATCHING_OVERHEAD -static -c wrapper.cpp $(LIBS)
	clang++ $(CXXFLAGS) $(INCLUDE) -static -c ./runtime/texas.bc
	llvm-ar -rv libruntime.a texas.o backtrace_asm.o pin_interface.o wrapper.o $(TBB_LIB)

wrapper_static_disableTexasOpt_disableClusterCallstack: runtime_noopt
	clang++ $(CXXFLAGS) -static -c backtrace_asm.S
	clang++ $(CXXFLAGS) -O0 -static -c pin_interface.cpp
	clang++ $(CXXFLAGS) $(CPPFLAGS) $(INCLUDE) -DMEMORYTOOL_DISABLE_CLUSTERCALLSTACK -DMEMORYTOOL_DISABLE_TRACKING_OPTIONS -static -c -emit-llvm wrapper.cpp $(LIBS)
	llvm-dis wrapper.bc
	clang++ $(CXXFLAGS) $(CPPFLAGS) $(INCLUDE) -DMEMORYTOOL_DISABLE_CLUSTERCALLSTACK -DMEMORYTOOL_DISABLE_TRACKING_OPTIONS -static -c wrapper.bc $(LIBS)
	clang++ $(CXXFLAGS) $(INCLUDE) -static -c ./runtime/texas.bc
	llvm-ar -rv libruntime.a texas.o backtrace_asm.o pin_interface.o wrapper.o $(TBB_LIB)

wrapper_static_disableClusterCallstack: runtime
	clang++ $(CXXFLAGS) -static -c backtrace_asm.S
	clang++ $(CXXFLAGS) -O0 -static -c pin_interface.cpp
	clang++ $(CXXFLAGS) $(CPPFLAGS) $(INCLUDE) -DMEMORYTOOL_DISABLE_CLUSTERCALLSTACK -static -c -emit-llvm wrapper.cpp $(LIBS)
	llvm-dis wrapper.bc
	clang++ $(CXXFLAGS) $(CPPFLAGS) $(INCLUDE) -DMEMORYTOOL_DISABLE_CLUSTERCALLSTACK -static -c wrapper.bc $(LIBS)
	clang++ $(CXXFLAGS) $(INCLUDE) -static -c ./runtime/texas.bc
	llvm-ar -rv libruntime.a texas.o backtrace_asm.o pin_interface.o wrapper.o $(TBB_LIB)

runtime_noopt:
	./setup.sh ; cd ./runtime ; make state_no_opt -j

runtime:
	./setup.sh ; cd ./runtime ; make

wrapper_static_btree: runtime_btree
	clang++ $(CPPFLAGS) $(INCLUDE) $(CXXFLAGS) -emit-llvm -c wrapper.cpp
	llvm-link ./runtime/texas.bc wrapper.bc -o runtime.bc
	clang++ $(CXXFLAGS) -static -c runtime.bc
	llvm-dis wrapper.bc
	llvm-ar -rv libruntime.a runtime.o

runtime_btree:
	./setup.sh ; cd ./runtime ; make b_tree

inject:
	clang++ -std=c++17 -shared -fPIC -ldl -o inject.so wrapper.cpp inject.cpp

clean:
	rm -f ./*.o ./*.bc ./*.ll ./*.so ./*.a ./*.txt
	if test -d "./runtime" ; then cd ./runtime ; make clean ; fi
	rm -f ./runtime

.PHONY: runtime runtime_noopt clean
