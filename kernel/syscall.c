/* syscall.c - initializes and handles syscalls */

/* Copyright (C) 2015 Bth8 <bth8fwd@gmail.com>
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
#include <fileops.h>
#include <vfs.h>
#include <elf.h>

DEFN_SYSCALL0(fork, 0);
DEFN_SYSCALL1(exit, 1, int32_t);
DEFN_SYSCALL0(getpid, 2);
DEFN_SYSCALL2(setpgid, 3, pid_t, pid_t);
DEFN_SYSCALL1(getpgid, 4, pid_t);
DEFN_SYSCALL0(setsid, 5);
DEFN_SYSCALL0(getsid, 6);
DEFN_SYSCALL1(nice, 7, int32_t);
DEFN_SYSCALL3(setresuid, 8, uid_t, uid_t, uid_t);
DEFN_SYSCALL3(getresuid, 9, uid_t*, uid_t*, uid_t*);
DEFN_SYSCALL3(setresgid, 10, gid_t, gid_t, gid_t);
DEFN_SYSCALL3(getresgid, 11, gid_t*, gid_t*, gid_t*);
DEFN64_SYSCALL4(lseek, 12, int32_t, uint32_t, uint32_t, int32_t);
DEFN64_SYSCALL6(pread, 13, int32_t, char*, uint32_t, uint32_t, uint32_t, uint32_t);
DEFN64_SYSCALL4(read, 14, int32_t, char*, uint32_t, uint32_t);
DEFN64_SYSCALL6(pwrite, 15, int32_t, const char*, uint32_t, uint32_t, uint32_t,
	uint32_t);
DEFN64_SYSCALL4(write, 16, int32_t, const char*, uint32_t, uint32_t);
DEFN_SYSCALL3(open, 17, const char*, uint32_t, mode_t);
DEFN_SYSCALL1(close, 18, int32_t);
DEFN_SYSCALL3(readdir, 19, int32_t, void*, uint32_t);
DEFN_SYSCALL2(stat, 20, const char*, void*);
DEFN_SYSCALL2(chmod, 21, int32_t, mode_t);
DEFN_SYSCALL3(chown, 22, int32_t, uid_t, gid_t);
DEFN_SYSCALL3(ioctl, 23, int, uint32_t, void*);
DEFN_SYSCALL2(link, 24, const char*, const char*);
DEFN_SYSCALL1(unlink, 25, const char*);
DEFN_SYSCALL3(mknod, 26, const char*, mode_t, dev_t);
DEFN_SYSCALL4(mount, 27, const char*, const char*, const char*, uint32_t);
DEFN_SYSCALL2(umount, 28, const char*, uint32_t);
DEFN_SYSCALL1(sbrk, 29, uintptr_t);
DEFN_SYSCALL3(execve, 30, const char*, char *const*, char *const*);
DEFN_SYSCALL1(chdir, 31, const char*);

static void *syscalls[] = {
	// Defined in task.c
	fork,
	exit_task,
	getpid,
	setpgid,
	getpgid,
	setsid,
	getsid,
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
	user_stat,
	user_chmod,
	user_chown,
	user_ioctl,
	user_link,
	user_unlink,
	mknod,
	user_mount,
	user_umount,
	sbrk,
	execve,
	chdir
};
uint32_t num_syscalls;

void syscall_handler(registers_t *regs) {
	if (regs->eax < num_syscalls) {
		void *location = syscalls[regs->eax];
		int32_t reta, retd;

		// Push all of the parameters
		asm volatile("pushl %2; \
				pushl %3; \
				pushl %4; \
				pushl %5; \
				pushl %6; \
				pushl %7; \
				call *%8; \
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
	ASSERT(register_interrupt_handler(0x80, &syscall_handler) == 0);
}
