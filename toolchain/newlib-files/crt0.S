.section .text

.global _start
_start:
	# Set up end of the stack frame linked list
	movl $0, %ebp
	pushl %ebp # eip = 0
	pushl %ebp # ebp = 0
	movl %esp, %ebp

	pushl %esi
	pushl %edi

	call initialize_standard_library
	call _init
	call _init_signal

	popl %edi
	popl %esi

	call main

	movl %eax, %edi
	call exit