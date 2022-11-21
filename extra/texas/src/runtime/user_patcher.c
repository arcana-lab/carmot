/*
   User-level CARAT patcher library for Linux

   Copyright (c) 2019 Peter A. Dinda

Protocol:

At startup, the process does:

carat_user_init();

This will register the process with the kernel module and it will
install a signal handler by which the kernel (or another user
process) can trigger a patch event.


carat_user_handler(int sig, siginfo_t *si, void *priv);

This is what is invoked on very thread on a patch event.  As 
the framework code shows, *priv is a ucontext / mcontext
which will give access to on-stack state for the interrupted
threads (registers, etc).

carat_user_deinit()

Called on shutdown to unregister with the kernel.

Compile with -DPRELOAD to get preload version

Notes:
1. Currently does not interact with the kernel
test using kill -12.
2. Because this code can be used with preload, 
we need to do system calls.
*/

#define _GNU_SOURCE
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <ucontext.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <sys/time.h>
#include <malloc.h>

#include "user_patcher.h"
#include "user_patcher_shared.h"


#define DEBUG_OUTPUT 1
#define NO_OUTPUT 0 

// example of how to invoke a syscall in a preload context
// where you should not rely on the ability to link to the
// syscall wrappers
static inline int gettid()
{
    return syscall(SYS_gettid);
}


void* (*old_malloc_hook)(size_t, const void*);
void* (*old_realloc_hook)(void*, size_t, const void*);
void (*old_free_hook)(void*, const void*);
void* (*old_memalign_hook)(size_t, size_t, const void*);


static volatile int inmalloc = 0;


static void *my_malloc_hook(size_t n, const void *addr)
{
    __sync_fetch_and_or(&inmalloc,1);
    __malloc_hook = old_malloc_hook;
    void *p = malloc(n);
    __malloc_hook = my_malloc_hook;
    __sync_fetch_and_and(&inmalloc,0);
    return p;
}

static void *my_realloc_hook(void *p, size_t n, const void *addr)
{
    __sync_fetch_and_or(&inmalloc,1);
    __realloc_hook = old_realloc_hook;
    void *np = realloc(p,n);
    __realloc_hook = my_realloc_hook;
    __sync_fetch_and_and(&inmalloc,0);
    return np;
}

static void my_free_hook(void *p, const void *addr)
{
    __sync_fetch_and_or(&inmalloc,1);
    __free_hook = old_free_hook;
    free(p);
    __free_hook = my_free_hook;
    __sync_fetch_and_and(&inmalloc,0);
}

static void* my_memalign_hook(size_t a, size_t n, const void *addr)
{
    __sync_fetch_and_or(&inmalloc,1);
    __memalign_hook = old_memalign_hook;
    void *p = memalign(a,n);
    __memalign_hook = my_memalign_hook;
    __sync_fetch_and_and(&inmalloc,0);
    return p;
}

static int install_my_malloc_hooks()
{
    old_malloc_hook = __malloc_hook;
    __malloc_hook = my_malloc_hook;

    old_realloc_hook = __realloc_hook;
    __realloc_hook = my_realloc_hook;

    old_free_hook = __free_hook;
    __free_hook = my_free_hook;

    old_memalign_hook = __memalign_hook;
    __memalign_hook = my_memalign_hook;

    return 0;
}

int malloc_safe()
{
    return !__sync_fetch_and_or(&inmalloc,0);
}


#if DEBUG_OUTPUT
#define DEBUG(S, ...) fprintf(stderr, "carat_user: debug(%8d): " S, gettid(), ##__VA_ARGS__)
#else 
#define DEBUG(S, ...) 
#endif

#if NO_OUTPUT
#define INFO(S, ...) 
#define ERROR(S, ...)
#else
#define INFO(S, ...) fprintf(stderr,  "carat_user: info(%8d): " S, gettid(), ##__VA_ARGS__)
#define ERROR(S, ...) fprintf(stderr, "carat_user: ERROR(%8d): " S, gettid(), ##__VA_ARGS__)
#endif

static struct sigaction oldsa_texas;

static int inited=0;

static void *heapstart=0;

static int find_heap()
{
    int pid = getpid();
    char buf[1024];
    sprintf(buf,"/proc/%d/maps",pid);
    FILE *f = fopen(buf,"r");
    if (!f) { 
        ERROR("Failed to open map file\n");
        return -1;
    } 
    while (!feof(f)) { 
        fgets(buf,1024,f);
        DEBUG("parsing %s",buf);
        uint64_t start,end;
        if (strstr(buf,"[heap]")) {
            DEBUG("found heap\n");
            if (sscanf(buf,"%lx-%lx",&start,&end)!=2) { 
                ERROR("Can't parse heap data\n");
                return -1;
            } else {
                fclose(f);
                heapstart = (void*)start;
                DEBUG("HEAP BEGINS AT %p\n",heapstart);
                return 0;
            } 
        }
    }
    ERROR("Cannot find heap section\n");
    return -1;
}

int safe_to_patch(void *begin)
{
    if (begin>=heapstart) { 
        return 1;
    } else {
        DEBUG("VETOING patch of %p\n",begin);
        return 0;
    }
}



#ifdef CARAT_PRELOAD

static int aborted=0;

static sighandler_t (*orig_signal)(int sig, sighandler_t func) = 0;
static int (*orig_sigaction)(int sig, const struct sigaction *act, struct sigaction *oldact) = 0;
static void (*orig_exit)(int) __attribute__((noreturn)) = 0;

#define ORIG_RETURN(func,...) if (orig_##func) { return orig_##func(__VA_ARGS__); } else { ERROR("cannot call orig_" #func " returning zero\n"); return 0; }
#define ORIG_IF_CAN(func,...) if (orig_##func) { if (!DEBUG_OUTPUT) { orig_##func(__VA_ARGS__); } else { DEBUG("orig_"#func" returns 0x%x\n",orig_##func(__VA_ARGS__)); } } else { DEBUG("cannot call orig_" #func " - skipping\n"); }

#else // ndef CARAT_PRELOAD

#define ORIG_RETURN(func,...) return func(__VA_ARGS__)
#define ORIG_IF_CAN(func,...) if (!DEBUG_OUTPUT) { func(__VA_ARGS__); } else { DEBUG(#func" returns 0x%x\n",func(__VA_ARGS__)); }

#endif // CARAT_PRELOAD

// call stack debugging

//#define SHOW_CALL_STACK() DEBUG("callstack (3 deep) : %p -> %p -> %p\n", __builtin_return_address(3), __builtin_return_address(2), __builtin_return_address(1))
//#define SHOW_CALL_STACK() DEBUG("callstack (2 deep) : %p -> %p\n", __builtin_return_address(2), __builtin_return_address(1))
//#define SHOW_CALL_STACK() DEBUG("callstack (1 deep) : %p\n", __builtin_return_address(1))
#define SHOW_CALL_STACK()



static inline uint64_t __attribute__((always_inline)) rdtsc(void)
{
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return lo | ((uint64_t)(hi) << 32);
}



#ifdef CARAT_PRELOAD
static __attribute__((constructor))
#endif
    void user_init(void);


#ifdef CARAT_PRELOAD

static void abort_operation(char *reason)
{
    if (!inited) {
        DEBUG("Initializing before abortingi\n");
        user_init();
        DEBUG("Done with carat_user_init()\n");
    }

    if (!aborted) {
        ORIG_IF_CAN(sigaction,CARAT_SIGNAL,&oldsa_texas,0);

        aborted = 1;
        DEBUG("Aborted operation because %s\n",reason);
    }
}

sighandler_t signal(int sig, sighandler_t func)
{
    DEBUG("signal(%d,%p)\n",sig,func);
    SHOW_CALL_STACK();
    if (sig==CARAT_SIGNAL) {
        abort_operation("target is using signal with our signal\n");
    } 
    ORIG_RETURN(signal,sig,func);
}

int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact)
{
    DEBUG("sigaction(%d,%p,%p)\n",sig,act,oldact);
    SHOW_CALL_STACK();
    if (sig==CARAT_SIGNAL) {
        abort_operation("target is using sigaction with our signal\n");
    }
    ORIG_RETURN(sigaction,sig,act,oldact);
}


static __attribute__((destructor))
    void user_deinit(void) ;

    __attribute__((noreturn))
void exit(int val)
{
    DEBUG("exit(%d)\n",val);
    SHOW_CALL_STACK();
    user_deinit();
    orig_exit(val);
}

#endif

typedef struct {
    void* oldAddr;
    void* newAddr;
    uint64_t length;
} regions;

typedef enum{
    PAGE_4K_GRANULARITY,
    ALLOCATION_GRANULARITY,
} granularity_t;

//int carat_entry(void*, void*, int, void*);
int texas_entry(void**, regions*, uint64_t*, granularity_t);

union pointerComps {
    void* ptr;
    uint64_t val;
    long int sval;
};


char* regNames[] = 
{
    "R8",
    "R9",
    "R10",
    "R11",
    "R12",
    "R13",
    "R14",
    "R15",
    "RSI",
    "RBP",
    "RBX",
    "RDX",
    "RAX",
    "RCX",
    "RSP",
    "RIP",
    "EFL",
    "CSG",           /* Actually short cs, gs, fs, __pad0.  */
    "ERR",
    "TRA",
    "OLD",
    "CR2",
};

static uint64_t rejects=0;


static void user_handler(int sig, siginfo_t *si,  void *priv)
{

    if (!__sync_fetch_and_or(&inited,0)) { 
        return;
    }

    if (!malloc_safe()) { 
        goto out_good;
    }

    //DEBUG("Entered carat_user_handler\n");
    ucontext_t *uc = (ucontext_t *)priv;

    uint64_t allocLen = 0;
    // fp regs are at uc->uc_mcontext.fpregs, e.g.:

#define OLD 0
#define NEW 1
    void* pointers[2];

    //This will be for a carat_move, I am going to let Carat move its largest alloc and tell us the return to change
    char name[1024];
    sprintf(name, "%s/.texas_mvmt", getenv("HOME"));
    int fd = open(name , O_RDONLY, 0);

    if(fd < 0){
        ERROR("No movement file!!!!!\n");
        return;
    }
    read(fd, pointers, sizeof(void*)*2);

    close(fd);

    DEBUG("Read out pointer: %p\n", pointers[0]);

    regions* patchedAllocs = NULL;
    uint64_t numAllocs = 0;

    if (texas_entry((void **)&pointers, &patchedAllocs, &numAllocs, PAGE_4K_GRANULARITY) == 1){
        ERROR("Carat entry failed\n");
        goto out_bad;
    }
    if (patchedAllocs == NULL){
        goto out_good;
    }



    for(int i = 0; i < numAllocs; i++){ 

        pointers[OLD] = patchedAllocs[i].oldAddr;
        pointers[NEW] = patchedAllocs[i].newAddr;

        union pointerComps oldAllocPtr, registerPtr, newAllocPtr, oldAllocPtrEnd;
        oldAllocPtr.ptr = (void*)pointers[OLD]; 
        allocLen = patchedAllocs[i].length;
        oldAllocPtrEnd.val = oldAllocPtr.val + allocLen;
        //Patch Registers now
        //DEBUG("Iterating %d (R8) through %d (CR2) for aliasing between %p to %p (span of %lu)\n", REG_R8, REG_CR2, oldAllocPtr.ptr, oldAllocPtrEnd.ptr, allocLen);
        for(int i = REG_R8; i <= REG_RIP; i++){
            registerPtr.val = uc->uc_mcontext.gregs[i];
            //DEBUG("Register %s contains %p\nComparing to %p - %p\n", regNames[i], registerPtr.ptr, oldAllocPtr.ptr, oldAllocPtrEnd.ptr);
            if(registerPtr.val >= oldAllocPtr.val && registerPtr.val < oldAllocPtrEnd.val){
                uint64_t offset = registerPtr.val - oldAllocPtr.val; 
                newAllocPtr.ptr = (void*) pointers[NEW];
                newAllocPtr.val += offset;
                //   DEBUG("It aliased %p will now become %p which is offset %ld\n", registerPtr.ptr, newAllocPtr.ptr, offset);

                if (safe_to_patch(patchedAllocs[i].oldAddr)) { 
                    uc->uc_mcontext.gregs[i] = newAllocPtr.val;
                }
                // DEBUG("The register %s is now %p\n", regNames[i], (void*) uc->uc_mcontext.gregs[i]);
            }
        }
    }


    for(int i = 0; i < numAllocs; i++){ 
        pointers[OLD] = patchedAllocs[i].oldAddr;
        pointers[NEW] = patchedAllocs[i].newAddr;
        allocLen = patchedAllocs[i].length;
        if(allocLen == 0){
            allocLen += 8;
        }
        memmove(pointers[NEW], pointers[OLD], allocLen);
        if (safe_to_patch(pointers[OLD])) { 
            free(pointers[OLD]);
        } else {
            // we capture the cost of the movement, but we don't use the target data
            DEBUG("NOT SAFE TO move %p\n", pointers[NEW]);
            free(pointers[NEW]);
        }
    }


out_good:
    return;

out_bad:
    return;


}


static int connect_to_kernel()
{
    DEBUG("connect to kernel\n");

    // WRITE ME

    return 0;
}

static int disconnect_from_kernel()
{
    DEBUG("disconnect to kernel\n");

    // WRITE ME

    return 0;
}

static void sigint_handler(int num)
{
    DEBUG("Interrupted - dumping state\n");
    exit(-1);
}



#ifdef CARAT_PRELOAD
#define SHIMIFY(x) if (!(orig_##x = dlsym(RTLD_NEXT, #x))) { DEBUG("Failed to setup SHIM for " #x "\n");  return; }
#else
#define SHIMIFY(x)
#endif

#ifdef CARAT_PRELOAD
static __attribute__((constructor))
#endif
void user_init(void) 
{
    INFO("init\n");
    if (!inited) {

        // wrap signal and sigaction for preload so that
        // we can capture changes to the carat signal
        // _exit is captured so that we can do deinit in all cases
        SHIMIFY(signal);
        SHIMIFY(sigaction);

        // invoke our deinit as the last thing the CRT
        // does during user cleanup
        atexit(user_deinit);

        if (connect_to_kernel()) {
            ERROR("Failed to connect to kernel\n");
            return;
        }

        if (find_heap()) { 
            ERROR("Unable to find heap\n");
            return;
        }

        struct sigaction sa;

        memset(&sa,0,sizeof(sa));
        sa.sa_sigaction = user_handler;
        sa.sa_flags |= SA_SIGINFO;

        ORIG_IF_CAN(sigaction,TEXAS_SIGNAL,&sa,&oldsa_texas);

        ORIG_IF_CAN(signal,SIGINT,sigint_handler);

        install_my_malloc_hooks();
        char name[1024];
        if(!getenv("HOME")){
            ERROR("Must set home variable!\n");
            return;
        }
        sprintf(name, "%s/.texas_pid", getenv("HOME"));
        int fd = open(name, O_WRONLY|O_CREAT, 0666);

        if(fd < 0){
            perror("Can't create file\n");
            ERROR("No pid file!!!!!\n");
            return;
        }
        int pid = getpid();
        write(fd, &pid, sizeof(pid));
        close(fd);

        __sync_fetch_and_or(&inited,1);


        DEBUG("Done with setup\n");
        return;
    } else {
        ERROR("already inited!\n");
        return;
    }
}



#ifdef CARAT_PRELOAD
static __attribute__((destructor))
#endif
void user_deinit(void) 
{ 
    // destroy the tracer thread
    // may not be seen if the FD is already closed
    DEBUG("deinit\n");

    if (inited
#ifdef CARAT_PRELOAD
            && !aborted
#endif
       ) {
        __sync_fetch_and_and(&inited,0);

        disconnect_from_kernel();
    }

    DEBUG("done\n");

}
