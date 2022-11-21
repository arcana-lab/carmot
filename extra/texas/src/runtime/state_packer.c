#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>


#include "state_packer.h"


#define HUGE_PAGE 1073741824
#define LARGE_PAGE 2097152
#define STANDARD_PAGE 4096


struct state_packer_state{

    uint64_t cache_line_size;
    uint64_t min_alignment;
    uint64_t current_offset;
    uint64_t page_alignment;
    uint64_t allocation_alignment;
    uint64_t allocation_size;
    void* allocation_ptr; 
};


uint64_t get_current_offset(struct state_packer_state* state){
    return state->current_offset;
}

/*
   Pack a sequence of allocations to maximize
   spatial locality for cache and to minimize number of PTEs
   */
struct state_packer_state *state_packer_new(uint64_t min_alignment,
        uint64_t cache_line_size){
    struct state_packer_state* newPacker = (struct state_packer_state*) calloc(sizeof(struct state_packer_state), 1);
    newPacker->min_alignment = min_alignment;
    newPacker->cache_line_size = cache_line_size;
    return newPacker;
}

//
// 1. To start a packing, call this function
// TODO: Set offset to 0
int state_packer_start_layout(struct state_packer_state *state){
    if(state == NULL){
        return 1;   
    }
    state->current_offset = 0;
    return 0;
}

//
// 2. Next, for each allocation, in the order you want to pack
//    call this function.   *target is where the packer
//    will write its intended offset for the allocation.
//    The caller should treat this as opaque
//TODO:
//  (1) - Give me length and alignment of alloc
//  (2) - Get max of struct and given alignment
//  (3) - Round current_offset up to multiple of (2)
//  (4) - if((3) is not a multiple of cache_line_size){
//           if ( (3) + len(allocation) extends past the end of current cache line){
//               round (3) up to next cache line
//           }
//        }
//  (5) - if ((4) is not a multiple of page_alignment){
//           if((4) + len allocation extends past the end of the page){
//              round (4) up to next page
//           }
//        }
//  (6) *target = (5)
//  (7) current_offset = (6) + len_of_allocation
int state_packer_add_allocation(struct state_packer_state *state,
        void                      *ptr_to_allocation,
        uint64_t                  len_of_allocation,
        uint64_t                  alignment_of_allocation,
        void                      **target){
    if(state == NULL){
        return 1;
    }
    //printf("Inside add allocation for: %p\n", ptr_to_allocation);
    uint64_t alignment = state->min_alignment;
    if(alignment_of_allocation > alignment){
        alignment = alignment_of_allocation;
    }
    if(!alignment){
        alignment = 8;
    }
    state->current_offset += (state->current_offset % alignment);
    if(!(state->cache_line_size)){
        state->cache_line_size = 64;
    }
    uint64_t currentOffsetIntoCacheline = state->current_offset % state->cache_line_size;
    if(currentOffsetIntoCacheline){
        if((len_of_allocation + currentOffsetIntoCacheline) > state->cache_line_size){
            state->current_offset += (state->cache_line_size - currentOffsetIntoCacheline);
        }
    }
    
    if(!(state->page_alignment)){
        state->page_alignment = 4096;
    }
    uint64_t currentOffsetIntoPage = state->current_offset % state->page_alignment;

    if(currentOffsetIntoPage){
        if((len_of_allocation + currentOffsetIntoPage) > state->page_alignment){
            state->current_offset += (state->page_alignment - currentOffsetIntoPage);
        }
    }

    *target = (void*) state->current_offset;

    state->current_offset += len_of_allocation;
    return 0;

}

//
// 3. Next call this to start doing the packing
//    This will allocate the needed memory for the new destinations
//
//TODO:
//1 - allocation_alignment = current_offset
//2 - allocation_size = current_offset
//3 - If( (1) > 1GB){
//      set (1) to 1GB
//      Round (2) to multiple of 1GB
//    }
//    else if ( (1) > 2MB){
//      set (1) to 2MB
//      Round (2) to multiple of 2MB
//    }
//    else{
//      set (1) to 4KB
//      Round (2) to multiple of 4KB
//    }
//4 - allocation_ptr = mmap(anonymous, target_address = some multiple of allocation_alignment, size = (2));
int state_packer_start_move(struct state_packer_state *state){
    if(state == NULL){
        return 1;
    }

    int protections = 0, flags = 0, fd = -1;
    off_t offset = 0;

    state->allocation_alignment = state->current_offset;
    state->allocation_size = state->current_offset;

    if(state->allocation_alignment > HUGE_PAGE){
        state->allocation_alignment = HUGE_PAGE;
        flags |= MAP_HUGETLB;
        //flags |= MAP_HUGETLB | MAP_HUGE_1GB;
    }
    else if(state->allocation_alignment > LARGE_PAGE){
        state->allocation_alignment = LARGE_PAGE;
        flags |= MAP_HUGETLB;
        //flags |= MAP_HUGETLB | MAP_HUGE_2MB;
    }
    else{
        state->allocation_alignment = STANDARD_PAGE; 
    }
    uint64_t alloc_offset = state->allocation_size % state->allocation_alignment;
    if(offset){
        state->allocation_size += (state->allocation_size - alloc_offset);
    }

    //Set up for mmap
    protections |= PROT_EXEC | PROT_READ | PROT_WRITE;
    flags |= MAP_SHARED | MAP_ANONYMOUS | MAP_POPULATE;


    state->allocation_ptr = mmap(NULL, (size_t)state->allocation_size, protections, flags, fd, offset);

    if(state->allocation_ptr == MAP_FAILED){
        return 1;
    }
    return 0;

}


int tgkill(int tgid, int tid, int signo)
{
    return syscall(SYS_tgkill,tgid, tid, signo);
}

int signal_process_threads(int pid, int signo)
{
    FILE *f;
    char cmd[80];
    int tid;

    sprintf(cmd,"ls -1 /proc/%d/task",pid);

    if (!(f=popen(cmd,"r"))) {
        printf("Can't open /proc/%d/task\n",pid);
        return -1;
    }

    while (fscanf(f,"%d",&tid)==1) {
        //printf("signaling thread %d\n",tid);
        tgkill(pid,tid,signo);
    }

    fclose(f);

    return 0;
}

//
// 4. Next, for each allocation, in the same order as in (2),
//    call this function.   *target is now the new address of the alloocation
//TODO:
//1 - *target += allocation_ptr 
//2 - memcpy(*target, ptr_to_allocation, length_of_allocation);
//3 - state_patch(ptr_to_allocation, *target);
int state_packer_move_allocation(struct state_packer_state *state,
        void                      *ptr_to_allocation,
        uint64_t                  len_of_allocation,
        void                      **target){
    if(state == NULL){
        return 1;
    }

    *target = (void*)(((uint64_t)(*target)) + ((uint64_t)(state->allocation_ptr)));
    memcpy(*target, ptr_to_allocation, len_of_allocation);
    //Implement a new patcher here? But we will need to patch our own registers, so maybe signal       ourselves and retrofit the signal handler to do this task too (involves providing the second addr to move stuff into for state_move)? 

    void* pointers[2];
    pointers[0] = ptr_to_allocation;
    pointers[1] = target;

    int fd = open("/home/suchy/.state_mvmt", O_RDWR|O_CREAT, 0666);
    if(fd < 0){
        printf("Bad fd\n");
        return 1;
    }
    write(fd, pointers, 2*sizeof(void*));
    close(fd);
    signal_process_threads(getpid(), 12);
    return 0;
}

//
// 5. Finally, call this to indicate completion
//
//TODO: Make this a state machine
int state_packer_finish(struct state_packer_state *state){return 0;}

//
// Call when you want to free the memory allocated at step 3
//
int state_packer_free_mem(struct state_packer_state *state){return 1;}

//
// Call when you want to free the whole context (but not the memory)
//
int state_packer_free(struct state_packer_state *state){return 1;}

//
// CURRRENT SERIOUS PROBLEM: WE CANNOT HANDLE A FREE FROM THE PROGRAM IN THIS INTERFACE
//










