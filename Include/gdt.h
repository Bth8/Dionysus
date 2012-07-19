/* gdt.h - data types and function declarations for GDT and TSS setup and management */
/* Copyright (C) 2011, 2012 Bth8 <bth8fwd@gmail.com>
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

#ifndef GDT_H
#define GDT_H
#include <common.h>

typedef struct gdt_entry_struct {
	u16int	limit_low;		// Lower 16 bits of the limit
	u16int	base_low;		// Lower 16 bits of the base
	u8int	base_mid;		// Middle 8 bits of the base
	u8int	access;			// Access byte. Contains access information. Check Intel documentation.
	u8int	granularity;	// middle 4 bits of limit | flags << 4
	u8int	base_high;		// High 8 bits of the base
} __attribute__((packed)) gdt_entry_t;

typedef struct gdt_ptr_struct {
	u16int	offset;			// Offset of last entry in table
	u32int	base;			// First entry in table
} __attribute__((packed)) gdt_ptr_t;

typedef struct tss_entry_struct {
	u16int prev_tss;		// Prev TSS. If we were hardware switching, this would help form a linked list
	u16int res0;
	u32int esp0;			// Stack pointer for kernel mode
	u16int ss0;				// Stack segment for kernel mode
	u16int res1;
	u32int esp1;			// These are unused...
	u16int ss1;
	u16int res2;
	u32int esp2;
	u16int ss2;
	u16int res3;
	u32int cr3;
	u32int eip;
	u32int eflags;
	u32int eax;
	u32int ecx;
	u32int edx;
	u32int ebx;
	u32int esp;
	u32int ebp;
	u32int esi;
	u32int edi;
	u16int es;				// Segments for kernel mode
	u16int res4;
	u16int cs;
	u16int res5;
	u16int ss;
	u16int res6;
	u16int ds;
	u16int res7;
	u16int fs;
	u16int res8;
	u16int gs;
	u16int res9;
	u16int ldt;				// Unused...
	u32int res10;
	u16int iomap_base;
} __attribute__((packed)) tss_entry_t;

void init_gdt(void);
void set_kernel_stack(u32int esp0);

#endif
