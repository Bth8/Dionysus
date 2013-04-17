/* paging.c - code for setting up and maintaining pages */
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

#include <common.h>
#include <paging.h>
#include <idt.h>
#include <kmalloc.h>
#include <printf.h>
#include <string.h>
#include <kheap.h>

// Defined in kmalloc.c
extern u32int placement_address;

// Defined in kheap.c
extern kheap_t *kheap;

// Defined in process.s
extern void copy_page_physical(u32int src, u32int dest);

// Bitset of frames
u32int *frames;
u32int nframes;

// Kernel page directory
page_directory_t *kernel_dir = NULL;

// Current page directory
page_directory_t *current_dir = NULL;

#define INDEX_FROM_BIT(a) (a/(8*4))
#define OFFSET_FROM_BIT(a) (a%(8*4))

static void set_frame(u32int addr) {
	u32int frame = addr / 0x1000;
	u32int i = INDEX_FROM_BIT(frame);
	u32int off = OFFSET_FROM_BIT(frame);
	frames[i] |= (0x01 << off);
}

static void clear_frame(u32int addr) {
	u32int frame = addr / 0x1000;
	u32int i = INDEX_FROM_BIT(frame);
	u32int off = OFFSET_FROM_BIT(frame);
	frames[i] &= ~(0x01 << off);
}

// Not actually used as of yet. May be used later. Commented out because compiler complains otherwise
/* static u32int test_frame(u32int addr) {
	u32int frame = addr / 0x1000;
	u32int i = INDEX_FROM_BIT(frame);
	u32int off = OFFSET_FROM_BIT(frame);
	return (frames[i] & (0x01 << off));
} */

static u32int first_frame(void) {
	u32int i, j;
	for (i = 0; i < INDEX_FROM_BIT(nframes); i++)
		if (frames[i] != 0xFFFFFFFF)
			for (j = 0; j < 32; j++) {
				u32int test = 0x01 << j;
				if (!(frames[i]&test))
					return i * 32 + j;
			}
	return 0xFFFFFFFF;
}

void alloc_frame(page_t *page, int kernel, int rw, int global) {
	if (page->frame != 0) // Already allocated
		return;

	u32int i;
	if ((i = first_frame()) == 0xFFFFFFFF)
		PANIC("No free frames.");
	set_frame(i*0x1000);
	page->present = 1;
	page->rw = (rw ? 1 : 0);
	page->user = (kernel ? 0 : 1);
	page->global = (global ? 1 : 0);
	page->frame = i;
}

void free_frame(page_t *page) {
	u32int frame;
	if (!(frame = page->frame))
		return;

	clear_frame(frame*0x1000);
	page->present = 0;
	page->frame = 0;
}

static void page_fault(registers_t *regs) {
	// Store faulting address
	u32int fault_addr;
	asm volatile("mov %%cr2, %0" : "=r" (fault_addr));

	// Output an error message.
	printf("Page fault! ( ");
	if (!(regs->err_code & 0x1)) {printf("present ");}	// Page not present
	if (regs->err_code & 0x2) {printf("read-only ");}	// Write operation
	if (regs->err_code & 0x4) {printf("user-mode ");}	// In user mode
	if (regs->err_code & 0x8) {printf("reserved ");}		// Reserved bits overwritten
	printf(") at 0x%X\n", fault_addr);
	PANIC("Page fault");
}

void init_paging(u32int mem_end) {
	mem_end &= 0xFFFFF000;		// Make sure it's page aligned. Shouldn't be a problem, but doesn't hurt
	nframes = mem_end / 0x1000;
	frames = (u32int *)kmalloc(INDEX_FROM_BIT(nframes));
	memset(frames, 0, INDEX_FROM_BIT(nframes));

	u32int i;																			
	kernel_dir = (page_directory_t *)kmalloc_ap(sizeof(page_directory_t), &i);
	memset(kernel_dir, 0, sizeof(page_directory_t));
	kernel_dir->physical_address = i;

	for (i = KHEAP_START; i < KHEAP_START + KHEAP_INIT_SIZE; i += 0x1000)
		get_page(i, 1, kernel_dir);

	// Identity page lowest MB
	for (i = 0; i < 0x100000; i += 0x1000)
		alloc_frame(get_page(i, 1, kernel_dir), 0, 0, 0);

	//Map kernel space to 0xC0000000
	for (i = 0xC0100000; i < placement_address + 0x1000; i += 0x1000)
		alloc_frame(get_page(i, 1, kernel_dir), 0, 0, 1);

	for (i = KHEAP_START; i < KHEAP_START + KHEAP_INIT_SIZE; i += 0x1000)
		alloc_frame(get_page(i, 1, kernel_dir), 0, 0, 1);

	register_interrupt_handler(14, page_fault);
	switch_page_dir(kernel_dir);

	kheap = create_heap(KHEAP_START, KHEAP_START + KHEAP_INIT_SIZE, 0xDFFFF000, 0, 1);	
	current_dir = clone_directory(kernel_dir);
	switch_page_dir(current_dir);
}

void switch_page_dir(page_directory_t *dir) {
	current_dir = dir;
	asm volatile("mov %0, %%cr3":: "r"(dir->physical_address));
}

page_t *get_page(u32int address, int make, page_directory_t *dir) {
	address /= 0x1000;
	u32int i = address / 1024;
	if (dir->tables[i]) // already assigned
		return &dir->tables[i]->pages[address%1024];
	else if (make) {
		u32int tmp;
		dir->tables[i] = (page_table_t*)kmalloc_ap(sizeof(page_table_t), &tmp);
		memset(dir->tables[i], 0, 0x1000);
		dir->tables_phys[i].present = 1;
		dir->tables_phys[i].rw = 1;
		dir->tables_phys[i].user = 1;
		dir->tables_phys[i].table = tmp >> 12;
		return &(dir->tables[i]->pages[address%1024]);
	} else
		return NULL;
}

static page_table_t *clone_table(page_table_t *src, u32int *physAddr) {
	page_table_t *table = (page_table_t *)kmalloc_ap(sizeof(page_table_t), physAddr);
	memset(table, 0, sizeof(page_table_t));
	int i;
	for (i = 0; i < 1024; i++) {
		if (src->pages[i].frame) {
			// Clear the frame
			table->pages[i].frame = 0;
			// Get a new frame
			alloc_frame(&table->pages[i], 0, 0, 0);
			// Copy flags
			table->pages[i].present = src->pages[i].present;
			table->pages[i].rw = src->pages[i].rw;
			table->pages[i].user = src->pages[i].user;
			table->pages[i].global = src->pages[i].global;
			table->pages[i].avail = src->pages[i].avail;
			// Copy the data in RAM across
			copy_page_physical(src->pages[i].frame * 0x1000, table->pages[i].frame * 0x1000);
		}
	}
	return table;
}

page_directory_t *clone_directory(page_directory_t *src) {
	u32int phys;
	page_directory_t *dir = (page_directory_t *)kmalloc_ap(sizeof(page_directory_t), &phys);
	memset(dir, 0, sizeof(page_directory_t));
	dir->physical_address = phys;
	int i;
	for (i = 0; i < 1024; i++) {
		if (src->tables[i]) {	// If it's already in the kernel directory (the first MB), link rather than copy.
			if (kernel_dir->tables[i] == src->tables[i]) {
				dir->tables[i] = src->tables[i];
				dir->tables_phys[i] = src->tables_phys[i];
			} else {
				u32int phys;
				dir->tables[i] = clone_table(src->tables[i], &phys);
				dir->tables_phys[i].present = 1;
				dir->tables_phys[i].rw = 1;
				dir->tables_phys[i].user = 1;
				dir->tables_phys[i].table = phys >> 12;
			}
		}
	}
	return dir;
}

static void free_table(page_table_t *table) {
	int i;
	for (i = 0; i < 1024; i++)
		free_frame(&table->pages[i]);
	kfree((void *)table);
}

void free_dir(page_directory_t *dir) {
	int i;
	for (i = 0; i < 1024; i++)
		if (dir->tables[i] && kernel_dir->tables[i] != dir->tables[i])
			free_table(dir->tables[i]);
	kfree((void *)dir);
}
	
