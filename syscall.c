/* syscall.c - initializes and handles syscalls */
/* Copyright (C) 2011 Bth8 <bth8fwd@gmail.com>
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
#include <monitor.h>
#include <idt.h>
#include <task.h>

DEFN_SYSCALL1(monitor_write, 0, const char*);
DEFN_SYSCALL1(monitor_write_hex, 1, const u32int);
DEFN_SYSCALL0(exit, 2);

static void *syscalls[] = {
	&monitor_write,
	&monitor_write_hex,
	&exit_task,
};
u32int num_syscalls;

void syscall_handler(registers_t regs) {
	asm volatile("mov %bx, %bx");
	if (regs.eax < num_syscalls) {
		void *location = syscalls[regs.eax];

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
					pop %%ebx;": "=a"(regs.eax) : "r"(regs.edi), "r"(regs.esi), "r"(regs.edx), "r"(regs.ecx), "r"(regs.ebx), "r"(location) : "ebx");
	}
}

void init_syscalls() {
	num_syscalls = sizeof(syscalls) / sizeof(void *);
	register_interrupt_handler(0x80, &syscall_handler);
}
