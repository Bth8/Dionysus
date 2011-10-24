/* common.h - Header for global data types/function declarations */
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

#ifndef COMMON_H
#define COMMON_H

#undef NULL
#define NULL ((void *)0)

#define PANIC(b) panic(__LINE__, __FILE__, b)
#define ASSERT(b) b ? NULL : PANIC("ASSERT failed")
#define BCD2HEX(bcd) ((bcd) = (((bcd) & 0xF0) >> 1) + (((bcd) & 0xF0) >> 3) + ((bcd) & 0xf))
#define READ_CMOS(addr) ({ \
outb(0x70, 0x80 | addr); \
inb(0x71); \
})
#define WRITE_CMOS(addr, val) ({ \
outb(0x70, 0x80 | addr); \
outb(0x71, val); \
})

typedef signed char		s8int;
typedef unsigned char	u8int;
typedef signed short	s16int;
typedef unsigned short	u16int;
typedef signed int		s32int;
typedef unsigned int	u32int;


// Bochs magic breakpoint. Doesn't actually do anything on a real system
extern inline void magic_break() { asm volatile("xchg %%bx, %%bx"::); }
extern inline void outb(u16int port, u8int value) { asm volatile("outb %1, %0":: "dN"(port), "a"(value)); }
extern inline void outw(u16int port, u16int value) { asm volatile("outw %1, %0":: "dN"(port), "a"(value)); }
extern inline void outl(u16int port, u32int value) { asm volatile("outl %1, %0":: "dN"(port), "a"(value)); }
// inb, inw, ind, insw and outsw are defined in port.s
u8int inb(u16int port);
u16int inw(u16int port);
u32int inl(u16int port);
void insw(u16int port, void *buf, int count);
void outsw(u16int port, void *src, int count);
extern inline void halt() {while(1){};}
//extern inline void halt() {__asm__("hlt");}
void panic(u32int line, char *file, char *msg);

#endif
