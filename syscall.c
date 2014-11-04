/* syscall.c - initializes and handles syscalls */

/* Copyright (C) 2014 Bth8 <bth8fwd@gmail.com>
 *
 *  This file is part of Dionysus.
 *
 *  Dionysus is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Dionysus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Dionysus.  If not, see <http://www.gnu.org/licenses/>
 */

#include <syscall.h>
#include <common.h>
#include <idt.h>
#include <task.h>
#include <vfs.h>
#include <elf.h>

DEFN_SYSCALL0(fork, 0);
DEFN_SYSCALL0(exit, 1);
DEFN_SYSCALL0(getpid, 2);
DEFN_SYSCALL1(nice, 3, int32_t);
DEFN_SYSCALL3(setresuid, 4, int32_t, int32_t, int32_t);
DEFN_SYSCALL3(getresuid, 5, int32_t*, int32_t*, int32_t*);
DEFN_SYSCALL3(setresgid, 6, int32_t, int32_t, int32_t);
DEFN_SYSCALL3(getresgid, 7, int32_t*, int32_t*, int32_t*);
DEFN64_SYSCALL4(lseek, 8, int32_t, uint32_t, uint32_t, int32_t);
DEFN64_SYSCALL6(pread, 9, int32_t, char*, uint32_t, uint32_t, uint32_t, uint32_t);
DEFN64_SYSCALL4(read, 10, int32_t, char*, uint32_t, uint32_t);
DEFN64_SYSCALL6(pwrite, 11, int32_t, const char*, uint32_t, uint32_t, uint32_t,
	uint32_t);
DEFN64_SYSCALL4(write, 12, int32_t, const char*, uint32_t, uint32_t);
DEFN_SYSCALL3(open, 13, const char*, uint32_t, uint32_t);
DEFN_SYSCALL1(close, 14, int32_t);
DEFN_SYSCALL3(readdir, 15, int32_t, void*, uint32_t);
DEFN_SYSCALL2(fstat, 16, int32_t, void*);
DEFN_SYSCALL2(chmod, 17, int32_t, uint32_t);
DEFN_SYSCALL3(chown, 18, int32_t, int32_t, int32_t);
DEFN_SYSCALL3(ioctl, 19, int, unsigned int, void*);
DEFN_SYSCALL1(unlink, 20, const char*);
DEFN_SYSCALL4(mount, 21, const char*, const char*, const char*, unsigned int);
DEFN_SYSCALL1(sbrk, 22, unsigned int);
DEFN_SYSCALL3(execve, 23, const char*, char *const*, char *const*);

static void *syscalls[] = {
	// Defined in task.c
	fork,
	exit_task,
	getpid,
	nice,
	setresuid,
	getresuid,
	setresgid,
	getresgid,
	lseek,
	user_pread,
	user_read,
	user_pwrite,
	user_write,
	user_open,
	user_close,
	user_readdir,
	user_fstat,
	user_chmod,
	user_chown,
	user_ioctl,
	user_unlink,
	user_mount,
	sbrk,
	execve
};
uint32_t num_syscalls;

void syscall_handler(registers_t *regs) {
	if (regs->eax < num_syscalls) {
		void *location = syscalls[regs->eax];
		int32_t reta, retd;

		// Push all of the parameters
		asm volatile("pushl %1; \
				pushl %2; \
				pushl %3; \
				pushl %4; \
				pushl %5; \
				pushl %6; \
				call *%7; \
				popl %%ebx; \
				popl %%ebx; \
				popl %%ebx; \
				popl %%ebx; \
				popl %%ebx; \
				popl %%ebx;" :
				"=a"(reta), "=d"(retd) :
				"g"(regs->ebp), "g"(regs->edi), "g"(regs->esi),
				"g"(regs->edx), "g"(regs->ecx), "g"(regs->ebx),
				"g"(location) :
				"ebx", "ecx");
		// cdecl states that eax, edx and ecx have to be callee saved.
		// eax is covered in the "=a" and edx in "=d".
		// We have to tell gcc about the rest.

		regs->eax = reta;
		regs->edx = retd;
	}
}

void init_syscalls(void) {
	num_syscalls = sizeof(syscalls) / sizeof(void *);
	register_interrupt_handler(0x80, &syscall_handler);
}
