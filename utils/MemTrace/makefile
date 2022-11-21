CXX=g++ -O3 -g

all: obj-intel64/memtrace.so

obj-intel64/%.o: %.cpp obj-intel64
	$(CXX) -Wall -Werror -Wno-unknown-pragmas -D__PIN__=1 -DPIN_CRT=1 -fno-stack-protector -fno-exceptions -funwind-tables -fasynchronous-unwind-tables -fno-rtti -DTARGET_IA32E -DHOST_IA32E -fPIC -DTARGET_LINUX -fabi-version=2 -faligned-new  -I${PIN_ROOT}/source/include/pin -I${PIN_ROOT}/source/include/pin/gen -isystem ${PIN_ROOT}/extras/stlport/include -isystem ${PIN_ROOT}/extras/libstdc++/include -isystem ${PIN_ROOT}/extras/crt/include -isystem ${PIN_ROOT}/extras/crt/include/arch-x86_64 -isystem ${PIN_ROOT}/extras/crt/include/kernel/uapi -isystem ${PIN_ROOT}/extras/crt/include/kernel/uapi/asm-x86 -I${PIN_ROOT}/extras/components/include -I${PIN_ROOT}/extras/xed-intel64/include/xed -I${PIN_ROOT}/source/tools/Utils -I${PIN_ROOT}/source/tools/InstLib -O3 -fomit-frame-pointer -fno-strict-aliasing   -c -o $@ $<

obj-intel64/memtrace.so: obj-intel64/memtrace.o obj-intel64/pin_interface.o #obj-intel64/wrapper.o obj-intel64/wrapper_internal.o 
	$(CXX) -shared -Wl,--hash-style=sysv ${PIN_ROOT}/intel64/runtime/pincrt/crtbeginS.o -Wl,-Bsymbolic -Wl,--version-script=${PIN_ROOT}/source/include/pin/pintool.ver -fabi-version=2    -o $@ $^  -L${PIN_ROOT}/intel64/runtime/pincrt -L${PIN_ROOT}/intel64/lib -L${PIN_ROOT}/intel64/lib-ext -L${PIN_ROOT}/extras/xed-intel64/lib -lpin -lxed ${PIN_ROOT}/intel64/runtime/pincrt/crtendS.o -lpin3dwarf  -ldl-dynamic -nostdlib -lstlport-dynamic -lm-dynamic -lc-dynamic -lunwind-dynamic

obj-intel64:
	mkdir $@

clean:
	rm -rf obj-intel64

.PHONY: all clean
