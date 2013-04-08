/* syscall.c - initializes and handles syscalls */
/* Copyright (C) 2011-2013 Bth8 <bth8fwd@gmail.com>
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

DEFN_SYSCALL0(fork, 0);
DEFN_SYSCALL0(exit, 1);
DEFN_SYSCALL0(getpid, 2);
DEFN_SYSCALL1(nice, 3, int);
DEFN_SYSCALL1(setuid, 4, int);
DEFN_SYSCALL1(seteuid, 5, int);
DEFN_SYSCALL2(setreuid, 6, int, int);
DEFN_SYSCALL3(setresuid, 7, int, int, int);
DEFN_SYSCALL0(getuid, 8);
DEFN_SYSCALL0(geteuid, 9);
DEFN_SYSCALL3(getresuid, 10, int*, int*, int*);
DEFN_SYSCALL1(setgid, 11, int);
DEFN_SYSCALL1(setegid, 12, int);
DEFN_SYSCALL2(setregid, 13, int, int);
DEFN_SYSCALL3(setresgid, 14, int, int, int);
DEFN_SYSCALL0(getgid, 15);
DEFN_SYSCALL0(getegid, 16);
DEFN_SYSCALL3(getresgid, 17, int*, int*, int*);
DEFN_SYSCALL2(open, 18, char*, u32int);
DEFN_SYSCALL1(close, 19, int);
DEFN_SYSCALL4(pread, 20, int, char*, size_t, off_t);
DEFN_SYSCALL3(read, 21, int, char*, size_t);
DEFN_SYSCALL4(pwrite, 22, int, char*, size_t, off_t);
DEFN_SYSCALL3(write, 23, int, char*, size_t);
DEFN_SYSCALL3(ioctl, 24, int, u32int, void*);
DEFN_SYSCALL3(lseek, 25, int, off_t, int);

static void *syscalls[] = {
	fork,		// Defined in task.c
	exit_task,
	getpid,
	nice,
	setuid,
	seteuid,
	setreuid,
	setresuid,
	getuid,
	geteuid,
	getresuid,
	setgid,
	setegid,
	setregid,
	setresgid,
	getgid,
	getegid,
	getresgid,
	user_open,
	user_close,
	user_pread,
	user_read,
	user_pwrite,
	user_write,
	user_ioctl,
	lseek
};
u32int num_syscalls;

void syscall_handler(registers_t *regs) {
	asm volatile("mov %bx, %bx");
	if (regs->eax < num_syscalls) {
		void *location = syscalls[regs->eax];

		// Push all of the parameters
		asm volatile("push %1;\
					push %2;\
					push %3;\
					push %4;\
					push %5;\
					call *%6;\
					pop %%ebx;\
					pop %%ebx;\
					pop %%ebx;\
					pop %%ebx;\
					pop %%ebx;": "=a"(regs->eax) : "r"(regs->edi), "r"(regs->esi), "r"(regs->edx), "r"(regs->ecx), "r"(regs->ebx), "r"(location) : "ebx");
	}
}

void init_syscalls(void) {
	num_syscalls = sizeof(syscalls) / sizeof(void *);
	register_interrupt_handler(0x80, &syscall_handler);
}
