/* syscall.h - stubs and declarations for syscalls */

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

#ifndef SYSCALL_H
#define SYSCALL_H

#include <common.h>
#include <vfs.h>
#include <task.h>

#define DECL_SYSCALL0(fn) int32_t sys_##fn(void);
#define DECL_SYSCALL1(fn,p1) int32_t sys_##fn(p1);
#define DECL_SYSCALL2(fn,p1,p2) int32_t sys_##fn(p1,p2);
#define DECL_SYSCALL3(fn,p1,p2,p3) int32_t sys_##fn(p1,p2,p3);
#define DECL_SYSCALL4(fn,p1,p2,p3,p4) int32_t sys_##fn(p1,p2,p3,p4);
#define DECL_SYSCALL5(fn,p1,p2,p3,p4,p5) int32_t sys_##fn(p1,p2,p3,p4,p5);
#define DECL_SYSCALL6(fn,p1,p2,p3,p4,p5,p6) int32_t sys_##fn(p1,p2,p3,p4,p5,p6);
#define DECL64_SYSCALL0(fn) int64_t sys_##fn(void);
#define DECL64_SYSCALL1(fn,p1) int64_t sys_##fn(p1);
#define DECL64_SYSCALL2(fn,p1,p2) int64_t sys_##fn(p1,p2);
#define DECL64_SYSCALL3(fn,p1,p2,p3) int64_t sys_##fn(p1,p2,p3);
#define DECL64_SYSCALL4(fn,p1,p2,p3,p4) int64_t sys_##fn(p1,p2,p3,p4);
#define DECL64_SYSCALL5(fn,p1,p2,p3,p4,p5) int64_t sys_##fn(p1,p2,p3,p4,p5);
#define DECL64_SYSCALL6(fn,p1,p2,p3,p4,p5,p6) int64_t sys_##fn(p1,p2,p3,p4,p5,p6);

#define DEFN_SYSCALL0(fn, num) \
int32_t sys_##fn(void) { \
	int32_t a; \
	asm volatile("int $0x80" : "=a"(a) : "0"(num)); \
	return a; \
}

#define DEFN_SYSCALL1(fn, num, P1) \
int32_t sys_##fn(P1 p1) { \
	int32_t a; \
	asm volatile("int $0x80" : "=a"(a) : "0"(num), "b"((uint32_t)p1)); \
	return a; \
}

#define DEFN_SYSCALL2(fn, num, P1, P2) \
int32_t sys_##fn(P1 p1, P2 p2) { \
	int32_t a; \
	asm volatile("int $0x80" : "=a"(a) : "0"(num), "b"((uint32_t)p1), \
			"c"((uint32_t)p2)); \
	return a; \
}

#define DEFN_SYSCALL3(fn, num, P1, P2, P3) \
int32_t sys_##fn(P1 p1, P2 p2, P3 p3) { \
	int32_t a; \
	asm volatile("int $0x80" : "=a"(a) : "0"(num), "b"((uint32_t)p1), \
			"c"((uint32_t)p2), "d"((uint32_t)p3)); \
	return a; \
}

#define DEFN_SYSCALL4(fn, num, P1, P2, P3, P4) \
int32_t sys_##fn(P1 p1, P2 p2, P3 p3, P4 p4) { \
	int32_t a; \
	asm volatile("int $0x80" : "=a"(a) : "0"(num), "b"((uint32_t)p1), \
			"c"((uint32_t)p2), "d"((uint32_t)p3), "S"((uint32_t)p4)); \
	return a; \
}

#define DEFN_SYSCALL5(fn, num, P1, P2, P3, P4, P5) \
int32_t sys_##fn(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) { \
	int32_t a; \
	asm volatile("int $0x80" : "=a"(a) : "0"(num), "b"((uint32_t)p1), \
			"c"((uint32_t)p2), "d"((uint32_t)p3), "S"((uint32_t)p4), \
			"D"((uint32_t)p5)); \
	return a; \
}

#define DEFN_SYSCALL6(fn, num, P1, P2, P3, P4, P5, P6) \
int32_t sys_##fn(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6) { \
	int32_t a; \
	asm volatile("pushl %%ebp; movl %7, %%ebp; int $0x80; popl %%ebp;" :\
			"=a"(a) : \
			"0"(num), "b"((uint32_t)p1), "c"((uint32_t)p2), "d"((uint32_t)p3), \
			"S"((uint32_t)p4), "D"((uint32_t)p5), "g"((uint32_t)p6)); \
	return a; \
}

#define DEFN64_SYSCALL0(fn, num) \
int64_t sys_##fn(void) { \
	int32_t a, d; \
	asm volatile("int $0x80" : "=a"(a), "=d"(d) : "0"(num)); \
	return ((int64_t)d << 32) | ((int64_t)a & 0xFFFFFFFF); \
}

#define DEFN64_SYSCALL1(fn, num, P1) \
int64_t sys_##fn(P1 p1) { \
	int32_t a, d; \
	asm volatile("int $0x80" : "=a"(a), "=d"(d) : "0"(num), "b"((uint32_t)p1)); \
	return ((int64_t)d << 32) | ((int64_t)a & 0xFFFFFFFF); \
}
#define DEFN64_SYSCALL2(fn, num, P1, P2) \
int64_t sys_##fn(P1 p1, P2 p2) { \
	int32_t a, d; \
	asm volatile("int $0x80" : "=a"(a), "=d"(d) : "0"(num), "b"((uint32_t)p1), \
			"c"((uint32_t)p2)); \
	return ((int64_t)d << 32) | ((int64_t)a & 0xFFFFFFFF); \
}

#define DEFN64_SYSCALL3(fn, num, P1, P2, P3) \
int64_t sys_##fn(P1 p1, P2 p2, P3 p3) { \
	int32_t a, d; \
	asm volatile("int $0x80" : "=a"(a), "=d"(d) : "0"(num), "b"((uint32_t)p1), \
			"c"((uint32_t)p2), "1"((uint32_t)p3)); \
	return ((int64_t)d << 32) | ((int64_t)a & 0xFFFFFFFF); \
}

#define DEFN64_SYSCALL4(fn, num, P1, P2, P3, P4) \
int64_t sys_##fn(P1 p1, P2 p2, P3 p3, P4 p4) { \
	int32_t a, d; \
	asm volatile("int $0x80" : "=a"(a), "=d"(d) : "0"(num), "b"((uint32_t)p1), \
			"c"((uint32_t)p2), "1"((uint32_t)p3), "S"((uint32_t)p4)); \
	return ((int64_t)d << 32) | ((int64_t)a & 0xFFFFFFFF); \
}

#define DEFN64_SYSCALL5(fn, num, P1, P2, P3, P4, P5) \
int64_t sys_##fn(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) { \
	int32_t a, d; \
	asm volatile("int $0x80" : "=a"(a), "=d"(d) : "0"(num), "b"((uint32_t)p1), \
			"c"((uint32_t)p2), "1"((uint32_t)p3), "S"((uint32_t)p4), \
			"D"((uint32_t)p5)); \
	return ((int64_t)d << 32) | ((int64_t)a & 0xFFFFFFFF); \
}

#define DEFN64_SYSCALL6(fn, num, P1, P2, P3, P4, P5, P6) \
int64_t sys_##fn(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6) { \
	int32_t a, d; \
	asm volatile("pushl %%ebp; movl %7, %%ebp; int $0x80; popl %%ebp;" :\
			"=a"(a), "=d"(d) : \
			"0"(num), "b"((uint32_t)p1), "c"((uint32_t)p2), "1"((uint32_t)p3), \
			"S"((uint32_t)p4), "D"((uint32_t)p5), "g"((uint32_t)p6)); \
	return ((int64_t)d << 32) | ((int64_t)a & 0xFFFFFFFF); \
}

DECL_SYSCALL0(fork);
DECL_SYSCALL1(exit, int32_t);
DECL_SYSCALL0(getpid);
DECL_SYSCALL2(setpgid, pid_t, pid_t);
DECL_SYSCALL1(getpgid, pid_t);
DECL_SYSCALL0(setsid);
DECL_SYSCALL0(getsid);
DECL_SYSCALL1(nice, int32_t);
DECL_SYSCALL3(setresuid, uid_t, uid_t, uid_t);
DECL_SYSCALL3(getresuid, uid_t*, uid_t*, uid_t*);
DECL_SYSCALL3(setresgid, gid_t, gid_t, gid_t);
DECL_SYSCALL3(getresgid, gid_t*, gid_t*, gid_t*);
DECL64_SYSCALL4(lseek, int32_t, uint32_t, uint32_t, int32_t);
DECL64_SYSCALL6(pread, int32_t, char*, uint32_t, uint32_t, uint32_t, uint32_t);
DECL64_SYSCALL4(read, int32_t, char*, uint32_t, uint32_t);
DECL64_SYSCALL6(pwrite, int32_t, const char*, uint32_t, uint32_t, uint32_t,
	uint32_t);
DECL64_SYSCALL4(write, int32_t, const char*, uint32_t, uint32_t);
DECL_SYSCALL3(open, const char*, uint32_t, mode_t);
DECL_SYSCALL1(close, int32_t);
DECL_SYSCALL3(readdir, int32_t, void*, uint32_t);
DECL_SYSCALL2(stat, const char*, void*);
DECL_SYSCALL2(chmod, int32_t, mode_t);
DECL_SYSCALL3(chown, int32_t, uid_t, gid_t);
DECL_SYSCALL3(ioctl, int, unsigned int, void*);
DECL_SYSCALL2(link, const char*, const char*);
DECL_SYSCALL1(unlink, const char*);
DECL_SYSCALL3(mknod, const char*, mode_t, dev_t);
DECL_SYSCALL4(mount, const char*, const char*, const char*, uint32_t);
DECL_SYSCALL2(umount, const char*, uint32_t);
DECL_SYSCALL1(sbrk, uintptr_t);
DECL_SYSCALL3(execve, const char*, char *const*, char *const*);
DECL_SYSCALL1(chdir, const char*);

void init_syscalls(void);

#endif /* SYSCALL_H */
