#define RET_CAP  ((1<<20)-1)
#define IPRETURN 4
#define CAPNUM   12

.globl SS_ipc_client_marshal_args
.type  SS_ipc_client_marshal_args, @function
.align 16
SS_ipc_client_marshal_args:
	/*
	 * The layout here is important.  If we change the registers
	 * pushed after the stack frame, we need to change the stack
	 * tracking functions.
	 */
	pushl %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	pushl %edx
	movl CAPNUM(%eax), %eax /* typeof(%eax) = (usr_inv_cap*) */

	/* arguments */
	movl 24(%esp), %ebx
	movl 28(%esp), %esi
	movl 32(%esp), %edi
	movl 36(%esp), %edx

	movl %esp, %ebp /* save the stack */
	movl $SS_ipc_client_ret_unmarshal, %ecx /* save the return ip */

	sysenter 

.align 8
	jmp SS_ipc_client_ret_fatal_err
.globl SS_ipc_client_ret_unmarshal
.align 8
SS_ipc_client_ret_unmarshal:
	popl %edx
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	
	ret
SS_ipc_client_ret_fatal_err:	
	popl %edx
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	
	movl $-0xDEAD, %eax
	ret
