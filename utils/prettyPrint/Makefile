TBBFLAGS= -I/opt/intel/tbb/include/ -Wl,-rpath,/opt/intel/tbb/lib/intel64_lin/gcc4.7 -L/opt/intel/tbb/lib/intel64_lin/gcc4.7 -ltbb
INCLUDE=-I../../include -I../../src/include -I../../include/runtime/virgil/include $(TBBFLAGS)
LIBS=-lboost_serialization -lboost_system

all: get-parallelfor get-referencecycles get-criticalsection prettyPrint

get-parallelfor: help
	clang++ -O3 -g $(INCLUDE) $@.cpp -c -o $@.o $(LIBS)
	clang++ -O3 -g $(INCLUDE) $@.o $<.o -o $@ $(LIBS)

get-referencecycles: help
	clang++ -O3 -g $(INCLUDE) $@.cpp -c -o $@.o $(LIBS)
	clang++ -O3 -g $(INCLUDE) $@.o $<.o -o $@ $(LIBS)

get-criticalsection: help
	clang++ -O3 -g $(INCLUDE) $@.cpp -c -o $@.o $(LIBS)
	clang++ -O3 -g $(INCLUDE) $@.o $<.o -o $@ $(LIBS)

get-accelerator: help
	clang++ -O3 -g $(INCLUDE) $@.cpp -c -o $@.o $(LIBS)
	clang++ -O3 -g $(INCLUDE) $@.o $<.o -o $@ $(LIBS)

get-stateprivatization: help
	clang++ -O3 -g $(INCLUDE) $@.cpp -c -o $@.o $(LIBS)
	clang++ -O3 -g $(INCLUDE) $@.o $<.o -o $@ $(LIBS)

prettyPrint: help
	clang++ -O3 -g $(INCLUDE) $@.cpp -c -o $@.o $(LIBS)
	clang++ -O3 -g $(INCLUDE) $@.o $<.o -o $@ $(LIBS)

get-ward: help
	clang++ -O3 -g $(INCLUDE) $@.cpp -c -o $@.o $(LIBS)
	clang++ -O3 -g $(INCLUDE) $@.o $<.o -o $@ $(LIBS)

help:
	clang++ -O3 -g $(INCLUDE) $@.cpp -c -o $@.o $(LIBS)

clean:
	rm -f get-referencecycles get-criticalsection get-accelerator get-stateprivatization prettyPrint

.PHONY: clean get-referencecycles get-criticalsection get-accelerator get-stateprivatization prettyPrint
