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
	uint16_t seg_limit_low; // Lower 16 bits of the limit
	uint16_t base_low; // Lower 16 bits of the base
	uint8_t base_mid; // Middle 8 bits of the base
	uint32_t seg_type : 4; // Read Intel documentation
	uint32_t desc_type : 1; // System if 0, code/data if 1
	uint32_t priv : 2;
	uint32_t present : 1;
	uint32_t seg_limit_high : 4;
	uint32_t avail : 1; // Whatever
	uint32_t reserved : 1;
	uint32_t DB : 1; // Changes depending on type
	uint32_t granularity : 1; // Limit can measure in increments of 1B to 4kiB
	uint8_t	base_high; // High 8 bits of the base
} __attribute__((packed)) gdt_entry_t;

typedef struct gdt_ptr_struct {
	uint16_t	offset; // Offset of last entry in table
	uint32_t	base; // First entry in table
} __attribute__((packed)) gdt_ptr_t;

typedef struct tss_entry_struct {
	uint16_t prev_tss; // Prev TSS. If we were hardware switching, this would
					 // help form a linked list
	uint16_t res0;
	uint32_t esp0; // Stack pointer for kernel mode
	uint16_t ss0; // Stack segment for kernel mode
	uint16_t res1;
	uint32_t esp1; // These are unused...
	uint16_t ss1;
	uint16_t res2;
	uint32_t esp2;
	uint16_t ss2;
	uint16_t res3;
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint16_t es; // Segments for kernel mode
	uint16_t res4;
	uint16_t cs;
	uint16_t res5;
	uint16_t ss;
	uint16_t res6;
	uint16_t ds;
	uint16_t res7;
	uint16_t fs;
	uint16_t res8;
	uint16_t gs;
	uint16_t res9;
	uint16_t ldt; // Unused...
	uint32_t res10;
	uint16_t iomap_base;
} __attribute__((packed)) tss_entry_t;

void init_gdt(void);
void set_kernel_stack(uint32_t esp0);

#endif /* GDT_H */
