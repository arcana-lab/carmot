/*
  Pack a sequence of allocations to maximize
  spatial locality for cache and to minimize number of PTEs
 */

struct state_packer_state* state_packer_new(uint64_t min_alignment,
					    uint64_t cache_line_size);


//Gets the current size of the superMalloc to be
uint64_t get_current_offset(struct state_packer_state* state);

//
// 1. To start a packing, call this function
//
int state_packer_start_layout(struct state_packer_state *state);

//
// 2. Next, for each allocation, in the order you want to pack
//    call this function.   *target is where the packer
//    will write its intended offset for the allocation.
//    The caller should treat this as opaque
int state_packer_add_allocation(struct state_packer_state *state,
				void                      *ptr_to_allocation,
				uint64_t                  len_of_allocation,
				uint64_t                  alignment_of_allocation,
				void                      **target);

//
// 3. Next call this to start doing the packing
//    This will allocate the needed memory for the new destinations
//
int state_packer_start_move(struct state_packer_state *state);

//
// 4. Next, for each allocation, in the same order as in (2),
//    call this function.   *target is now the new address of the alloocation
//
int state_packer_move_allocation(struct state_packer_state *state,
				 void                      *ptr_to_allocation,
				 uint64_t                  len_of_allocation,
				 void                      **target);

//
// 5. Finally, call this to indicate completion
//
int state_packer_finish(struct state_packer_state *state);

//
// Call when you want to free the memory allocated at step 3
//
int state_packer_free_mem(struct state_packer_state *state);

//
// Call when you want to free the whole context (but not the memory)
//
int state_packer_free(struct state_packer_state *state);

//
// CURRRENT SERIOUS PROBLEM: WE CANNOT HANDLE A FREE FROM THE PROGRAM IN THIS INTERFACE
//










