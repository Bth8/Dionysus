/* syscall.h - stubs and declarations for syscalls */
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

#ifndef SYSCALL_H
#define SYSCALL_H
#include <common.h>
#include <vfs.h>

#define DECL_SYSCALL0(fn) int sys_##fn(void);
#define DECL_SYSCALL1(fn,p1) int sys_##fn(p1);
#define DECL_SYSCALL2(fn,p1,p2) int sys_##fn(p1,p2);
#define DECL_SYSCALL3(fn,p1,p2,p3) int sys_##fn(p1,p2,p3);
#define DECL_SYSCALL4(fn,p1,p2,p3,p4) int sys_##fn(p1,p2,p3,p4);
#define DECL_SYSCALL5(fn,p1,p2,p3,p4,p5) int sys_##fn(p1,p2,p3,p4,p5);

#define DEFN_SYSCALL0(fn, num) \
int sys_##fn(void) { \
	int a; \
	asm volatile("int $0x80" : "=a"(a) : "0"(num)); \
	return a; \
}
#define DEFN_SYSCALL1(fn, num, P1) \
int sys_##fn(P1 p1) { \
	int a; \
	asm volatile("int $0x80" : "=a"(a) : "0"(num), "b"((int)p1)); \
	return a; \
}
#define DEFN_SYSCALL2(fn, num, P1, P2) \
int sys_##fn(P1 p1, P2 p2) { \
	int a; \
	asm volatile("int $0x80" : "=a"(a) : "0"(num), "b"((int)p1), "c"((int)p2)); \
	return a; \
}
#define DEFN_SYSCALL3(fn, num, P1, P2, P3) \
int sys_##fn(P1 p1, P2 p2, P3 p3) { \
	int a; \
	asm volatile("int $0x80" : "=a"(a) : "0"(num), "b"((int)p1), "c"((int)p2), "d"((int)p3)); \
	return a; \
}
#define DEFN_SYSCALL4(fn, num, P1, P2, P3, P4) \
int sys_##fn(P1 p1, P2 p2, P3 p3, P4 p4) { \
	int a; \
	asm volatile("int $0x80" : "=a"(a) : "0"(num), "b"((int)p1), "c"((int)p2), "d"((int)p3), "S"((int)p4)); \
	return a; \
}
#define DEFN_SYSCALL5(fn, num, P1, P2, P3, P4, P5) \
int sys_##fn(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) { \
	int a; \
	asm volatile("int $0x80" : "=a"(a) : "0"(num), "b"((int)p1), "c"((int)p2), "d"((int)p3), "S"((int)p4), "D"((int)p5)); \
	return a; \
}

DECL_SYSCALL0(fork);
DECL_SYSCALL0(exit);
DECL_SYSCALL0(getpid);
DECL_SYSCALL1(nice, int);
DECL_SYSCALL1(setuid, int);
DECL_SYSCALL1(seteuid, int);
DECL_SYSCALL2(setreuid, int, int);
DECL_SYSCALL3(setresuid, int, int, int);
DECL_SYSCALL0(getuid);
DECL_SYSCALL0(geteuid);
DECL_SYSCALL3(getresuid, int*, int*, int*);
DECL_SYSCALL1(setgid, int);
DECL_SYSCALL1(setegid, int);
DECL_SYSCALL2(setregid, int, int);
DECL_SYSCALL3(setresgid, int, int, int);
DECL_SYSCALL0(getgid);
DECL_SYSCALL0(getegid);
DECL_SYSCALL3(getresgid, int*, int*, int*);
DECL_SYSCALL3(open, const char*, unsigned int, unsigned int);
DECL_SYSCALL1(close, int);
DECL_SYSCALL4(pread, int, char*, size_t, off_t);
DECL_SYSCALL3(read, int, char*, size_t);
DECL_SYSCALL4(pwrite, int, const char*, size_t, off_t);
DECL_SYSCALL3(write, int, const char*, size_t);
DECL_SYSCALL3(ioctl, int, unsigned int, void*);
DECL_SYSCALL3(lseek, int, off_t, int);
DECL_SYSCALL4(mount, const char*, const char*, const char*, unsigned int);
DECL_SYSCALL3(readdir, int, void*, unsigned int);
DECL_SYSCALL2(fstat, int, void*);
DECL_SYSCALL1(unlink, const char*);

void init_syscalls(void);

#endif
