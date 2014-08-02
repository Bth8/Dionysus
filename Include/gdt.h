/* gdt.h - data types and function declarations for GDT and TSS setup
 * and management */

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

#ifndef GDT_H
#define GDT_H
#include <common.h>

#define GDT_SEGMENT_SYSTEM 0x0000
#define GDT_SEGMENT_DATA 0x0010
#define GDT_SEGMENT_CODE 0x0018
#define GDT_SEGMENT_PRESENT 0x0080
#define GDT_SEGMENT_GRANULAR 0x0800

#define GDT_DPL_RING0 0x0000
#define GDT_DPL_RING1 0x0020
#define GDT_DPL_RING2 0x0040
#define GDT_DPL_RING3 0x0060

#define GDT_DATA_ACCESSED 0x0001
#define GDT_DATA_WRITE 0x0002
#define GDT_DATA_DIRECTION 0x0004
#define GDT_DATA_32BIT 0x0400

#define GDT_CODE_ACCESSED 0x0001
#define GDT_CODE_READ 0x0002
#define GDT_CODE_CONFORMING 0x0004
#define GDT_CODE_32BIT 0x0400

// System types vary quite a bit more depending on other settings
// We'll only list the ones we use
#define GDT_SYSTEM_TSS 0x0001
#define GDT_SYSTEM_32BIT 0x0008
#define GDT_SYSTEM_BIG 0x0400


typedef struct gdt_entry_struct {
	u16int seg_limit_low; // Lower 16 bits of the limit
	u16int base_low; // Lower 16 bits of the base
	u8int base_mid; // Middle 8 bits of the base
	u32int seg_type : 4; // Read Intel documentation
	u32int desc_type : 1; // System if 0, code/data if 1
	u32int priv : 2;
	u32int present : 1;
	u32int seg_limit_high : 4;
	u32int avail : 1; // Whatever
	u32int reserved : 1;
	u32int DB : 1; // Changes depending on type
	u32int granularity : 1; // Limit can measure in increments of 1B to 4kiB
	u8int	base_high; // High 8 bits of the base
} __attribute__((packed)) gdt_entry_t;

typedef struct gdt_ptr_struct {
	u16int	offset; // Offset of last entry in table
	u32int	base; // First entry in table
} __attribute__((packed)) gdt_ptr_t;

typedef struct tss_entry_struct {
	u16int prev_tss; // Prev TSS. If we were hardware switching, this would
					 // help form a linked list
	u16int res0;
	u32int esp0; // Stack pointer for kernel mode
	u16int ss0; // Stack segment for kernel mode
	u16int res1;
	u32int esp1; // These are unused...
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
	u16int es; // Segments for kernel mode
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
	u16int ldt; // Unused...
	u32int res10;
	u16int iomap_base;
} __attribute__((packed)) tss_entry_t;

void init_gdt(void);
void set_kernel_stack(u32int esp0);

#endif /* GDT_H */
