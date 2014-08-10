/* paging.c - code for setting up and maintaining pages */

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

#include <common.h>
#include <paging.h>
#include <idt.h>
#include <printf.h>
#include <string.h>
#include <task.h>
#include <kmalloc.h>

// Defined in main.c
extern uint32_t placement_address;

uint32_t kheap = 0;

// Defined in process.s
extern void copy_page_physical(uint32_t src, uint32_t dest);

// Bitset of frames
uint32_t *frames;
uint32_t nframes;

// Kernel page directory
page_directory_t *kernel_dir = NULL;

// Current page directory
page_directory_t *current_dir = NULL;

#define INDEX_FROM_BIT(a) (a >> 5)
#define OFFSET_FROM_BIT(a) (a & 0x1F)

volatile uint8_t frame_lock = 0;

static void set_frame(uint32_t addr) {
	uint32_t frame = addr / PAGE_SIZE;
	uint32_t i = INDEX_FROM_BIT(frame);
	uint32_t off = OFFSET_FROM_BIT(frame);
	frames[i] |= (0x01 << off);
}

static void clear_frame(uint32_t addr) {
	uint32_t frame = addr / PAGE_SIZE;
	uint32_t i = INDEX_FROM_BIT(frame);
	uint32_t off = OFFSET_FROM_BIT(frame);
	frames[i] &= ~(0x01 << off);
}

/* static uint32_t test_frame(uint32_t addr) {
	uint32_t frame = addr / PAGE_SIZE;
	uint32_t i = INDEX_FROM_BIT(frame);
	uint32_t off = OFFSET_FROM_BIT(frame);
	return (frames[i] & (0x01 << off));
} */

static uint32_t first_frame(void) {
	uint32_t i, j;
	for (i = 0; i < INDEX_FROM_BIT(nframes); i++)
		if (frames[i] != 0xFFFFFFFF)
			for (j = 0; j < 32; j++) {
				uint32_t test = 0x01 << j;
				if (!(frames[i]&test))
					return (i << 5) + j;
			}
	return 0xFFFFFFFF;
}

void alloc_frame(page_t *page, int kernel, int rw) {
	if (page->frame != 0) // Already allocated
		return;

	spin_lock(&frame_lock);
	uint32_t i;
	if ((i = first_frame()) == 0xFFFFFFFF)
		PANIC("No free frames.");
	set_frame(i * PAGE_SIZE);
	page->present = 1;
	page->rw = (rw ? 1 : 0);
	page->user = (kernel ? 0 : 1);
	page->global = 0;
	page->frame = i;
	spin_unlock(&frame_lock);
}

// Directly map a page
void dm_frame(page_t *page, int kernel, int rw, uintptr_t addr) {
	spin_lock(&frame_lock);
	if ((addr / PAGE_SIZE) < nframes)
		set_frame(addr);
	page->present = 1;
	page->rw = rw ? 1 : 0;
	page->user = kernel ? 0 : 1;
	page->global = 0;
	page->frame = addr / PAGE_SIZE;
	spin_unlock(&frame_lock);
}

void free_frame(page_t *page) {
	uint32_t frame;
	if (!(frame = page->frame))
		return;

	clear_frame(frame * PAGE_SIZE);
	page->present = 0;
	page->frame = 0;
}

uintptr_t resolve_physical(uintptr_t addr) {
	if (kheap) {
		page_t *page = get_page(addr, 0, current_dir);
		return (page->frame * PAGE_SIZE) + (addr % PAGE_SIZE);
	} else {
		return addr - KERNEL_BASE;
	}
}

static void page_fault(registers_t *regs) {
	// Store faulting address
	uint32_t fault_addr;
	asm volatile("mov %%cr2, %0" : "=r" (fault_addr));

	// Output an error message.
	printf("Page fault! ( ");
	if (!(regs->err_code & 0x1)) {printf("present ");}
	if (regs->err_code & 0x2) {printf("read-only ");}
	if (regs->err_code & 0x4) {printf("user-mode ");}
	if (regs->err_code & 0x8) {printf("reserved ");}
	printf(") at 0x%X\n", fault_addr);
	PANIC("Page fault");
}

inline void *paging_memalign(size_t alignment, size_t size) {
	if (kheap)
		return kmemalign(alignment, size);
	else {
		uintptr_t diff = placement_address & (alignment - 1);
		if (diff != 0) {
			diff = alignment - diff;
			placement_address += diff;
		}

		uintptr_t tmp = placement_address;
		placement_address += size;

		return (void *)tmp;
	}
}

// memlength is a measure of the available memory in kilobytes
void init_paging(uint32_t memlength, uintptr_t mmap_addr,
		uintptr_t mmap_length) {
	nframes = memlength / 4;
	frames = (uint32_t *)paging_memalign(16, INDEX_FROM_BIT(nframes));
	memset(frames, 0, INDEX_FROM_BIT(nframes));

	kernel_dir = (page_directory_t *)paging_memalign(PAGE_SIZE, 
		sizeof(page_directory_t));
	memset(kernel_dir, 0, sizeof(page_directory_t));
	kernel_dir->physical_address = resolve_physical((uintptr_t)kernel_dir);

	multiboot_memory_map_t *mmap = (void *)mmap_addr;

	uintptr_t i;

	while ((uintptr_t)mmap < mmap_addr + mmap_length) {
		if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE)
			for (i = 0; i < mmap->len_low; i += PAGE_SIZE) {
				if ((mmap->addr_low + i) / PAGE_SIZE > nframes) break;
				set_frame(mmap->addr_low + i);
			}

		mmap = (void*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
	}

	// Reserve heap pages to make sure they're buried deep in kernel space
	for (i = KHEAP_START; i < KHEAP_MAX; i += PAGE_SIZE)
		get_page(i, 1, kernel_dir);

	// Identity page lowest MB. We shouldn't write to it
	for (i = 0; i < 0x100000; i += PAGE_SIZE)
		dm_frame(get_page(i, 1, kernel_dir), 1, 1, i);

	//Map kernel space to 0xC0000000
	for (i = 0xC0100000; i < placement_address + PAGE_SIZE; i += PAGE_SIZE)
		dm_frame(get_page(i, 1, kernel_dir), 1, 1, i - KERNEL_BASE);

	register_interrupt_handler(14, page_fault);
	switch_page_dir(kernel_dir);

	kheap = 1;
	current_dir = clone_directory(kernel_dir);
	switch_page_dir(current_dir);
}

void switch_page_dir(page_directory_t *dir) {
	current_dir = dir;
	asm volatile("mov %0, %%cr3":: "r"(dir->physical_address));
}

// Flush the entire TLB, ensuring global page refresh
void global_flush(void) {
	asm volatile("mov %%cr4, %%eax; btr $7, %%eax; mov %%eax, %%cr4;"
				"bts $7, %%eax; mov %%eax, %%cr4" : : : "eax");
}

page_t *get_page(uint32_t address, int make, page_directory_t *dir) {
	address /= PAGE_SIZE;
	uint32_t i = address / 1024;
	if (dir->tables[i]) // already assigned
		return &dir->tables[i]->pages[address%1024];
	else if (make) {
		dir->tables[i] = (page_table_t*)paging_memalign(PAGE_SIZE, 
			sizeof(page_table_t));
		memset(dir->tables[i], 0, sizeof(page_table_t));
		dir->tables_phys[i].present = 1;
		dir->tables_phys[i].rw = 1;
		dir->tables_phys[i].user = 1;
		dir->tables_phys[i].table = 
			(resolve_physical((uintptr_t)dir->tables[i]) >> 12);
		return &(dir->tables[i]->pages[address%1024]);
	} else
		return NULL;
}

static page_table_t *clone_table(page_table_t *src, uint32_t *physAddr) {
	page_table_t *table = (page_table_t *)paging_memalign(PAGE_SIZE,
		sizeof(page_table_t));
	*physAddr = resolve_physical((uintptr_t)table);
	memset(table, 0, sizeof(page_table_t));
	int i;
	for (i = 0; i < 1024; i++) {
		if (src->pages[i].frame) {
			// Clear the frame
			table->pages[i].frame = 0;
			// Get a new frame
			alloc_frame(&table->pages[i], 0, 0);
			// Copy flags
			table->pages[i].present = src->pages[i].present;
			table->pages[i].rw = src->pages[i].rw;
			table->pages[i].user = src->pages[i].user;
			table->pages[i].global = src->pages[i].global;
			table->pages[i].avail = src->pages[i].avail;
			// Copy the data in RAM across
			copy_page_physical(src->pages[i].frame * PAGE_SIZE,
					table->pages[i].frame * PAGE_SIZE);
		}
	}
	return table;
}

page_directory_t *clone_directory(page_directory_t *src) {
	page_directory_t *dir = (page_directory_t *)paging_memalign(PAGE_SIZE, 
		sizeof(page_directory_t));
	uintptr_t phys = resolve_physical((uintptr_t)dir);
	memset(dir, 0, sizeof(page_directory_t));
	dir->physical_address = phys;
	int i;
	for (i = 0; i < 1024; i++) {
		if (src->tables[i]) {
			// If it's already in the kernel directory (the first MB),
			// link rather than copy.
			if (kernel_dir->tables[i] == src->tables[i]) {
				dir->tables[i] = src->tables[i];
				dir->tables_phys[i] = src->tables_phys[i];
			} else {
				uint32_t phys;
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

