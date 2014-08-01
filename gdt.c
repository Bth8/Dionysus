/* gdt.c - Functions for setting up the GDT and TSS */

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

#include <gdt.h>
#include <common.h>
#include <string.h>

// Defined in descriptor_tables.s
extern void gdt_flush(u32int);
extern void tss_flush(void);

gdt_entry_t gdt_entries[6];
gdt_ptr_t gdt_ptr;
tss_entry_t tss_entry;

static void gdt_set_gate(u32int num, u32int base, u32int limit, u8int access,
		u8int gran) {
	gdt_entries[num].base_low = base & 0xFFFF;
	gdt_entries[num].base_mid = (base >> 16) & 0xFF;
	gdt_entries[num].base_high = base >> 24;

	gdt_entries[num].limit_low = limit & 0xFFFF;
	gdt_entries[num].granularity = (limit >> 16) & 0x0F;

	gdt_entries[num].access = access;

	gdt_entries[num].granularity |= gran & 0xF0;
}

static void write_tss(u32int num, u16int ss0, u16int esp0) {
	// Find base and limit of our TSS and set gates appropriately
	u32int base = (u32int)&tss_entry;
	u32int limit = base + sizeof(tss_entry_t);
	gdt_set_gate(num, base, limit, 0xE9, 0x00);

	// Zero it out
	memset(&tss_entry, 0, sizeof(tss_entry_t));

	// Set kernel stack segment and pointer
	tss_entry.ss0 = ss0;
	tss_entry.esp0 = esp0;

	// Set the kernel code and data segments (0x08, 0x10) with RPL 3 so we
	// can switch here from user mode
	tss_entry.cs = 0x0B;
	tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = 0x13;
}

void init_gdt(void) {
	gdt_ptr.offset = (sizeof(gdt_entry_t) * 6) - 1;
	gdt_ptr.base = (u32int)&gdt_entries;

	gdt_set_gate(0, 0, 0, 0, 0);	// Null segment required by Intel
	gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);	// Code segment
	gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);	// Data segment
	gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User code segment
	gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User data segment
	write_tss(5, 0x10, 0x0);

	gdt_flush((u32int)&gdt_ptr);
	tss_flush();
}

void set_kernel_stack(u32int esp0) {
	tss_entry.esp0 = esp0;
}
