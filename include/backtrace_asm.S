	//
	// extern "C" uint64_t backtrace_asm(void **trace, uint64_t max_depth, uint64_t temp1, uint64_t temp2)
	//
	// rax = return + temp
	// rdi = trace (curptr)
	// rsi = max_depth (assume >=1)
	// rdx = temp1 (we own this reg) [cur frame pointer]
	// rcx = temp2 (we own this reg) [count]
	//
	// Code assumes at least one stack frame exists
	// Code assumes that base pointers are always saved on stack
	//
	// On entry:
	//
	//  rbp => oldrbp on stack, +8 is return
.global backtrace_asm
backtrace_asm:
	movq %rbp, %rdx	  	// rdx = fp (current frame pointer), so we don't disturb rbp
	xor %rcx, %rcx 		// count = 0

top:	
	movq 8(%rdx), %rax       // get next return address, ret = *(fp+1)
	movq %rax, (%rdi)        // *trace = ret
	add $8, %rdi		// trace++
	inc %rcx		// count++
	dec %rsi		// max_depth--
	je  out                 // if max_depth==0, leave (out of room)
	
	movq (%rdx), %rdx	// fp = *fp//
	//test %rdx, %rdx
  cmp %rdx, %rsp
	jle top                 // if (fp) continue;  leave if out of frames

out:
	movq %rcx, %rax         	// return cout
	ret
	
	
	
	
