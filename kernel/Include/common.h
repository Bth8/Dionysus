/* common.h - Header for global data types/function declarations */

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

#ifndef COMMON_H
#define COMMON_H

#undef NULL
#define NULL ((void *)0)

#define PANIC(b) panic(__LINE__, __FILE__, b)
#define ASSERT(b) (b) ? NULL : PANIC("ASSERT failed\n\t" #b)
#define BCD2HEX(bcd) ((bcd) = (((bcd) & 0xF0) >> 1) + \
	(((bcd) & 0xF0) >> 3) + ((bcd) & 0xf))
#define offsetof(type, member) ((uintptr_t)(&((type *)NULL)->member))
#define container_of(ptr, type, member) ({ \
	const typeof(((type *)NULL)->member) *__mptr = (ptr); \
	(type *)((void *)__mptr - offsetof(type, member)); })

#include <stdint.h>

typedef uint64_t size_t;
typedef int64_t ssize_t;
typedef uint32_t dev_t;
typedef uint32_t uid_t;
typedef uint32_t gid_t;
typedef uint32_t ino_t;
typedef uint32_t mode_t;
typedef uint32_t nlink_t;
typedef int64_t off_t;
typedef int32_t pid_t;

typedef uint8_t spinlock_t;

// Bochs magic breakpoint. Doesn't actually do anything on a real system
void magic_break(void);
void outb(uint16_t port, uint8_t value);
void outw(uint16_t port, uint16_t value);
void outl(uint16_t port, uint32_t value);

// inb, inw, ind, insw and outsw are defined in port.s
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);
void insw(uint16_t port, void *buf, int count);
void outsw(uint16_t port, void *src, int count);

void halt(void);
void panic(uint32_t line, char *file, char *msg);
void spin_lock(volatile spinlock_t *lock);
void spin_unlock(volatile spinlock_t *lock);

#endif /* COMMON_H */
