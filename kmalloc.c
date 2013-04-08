/* kmalloc.c - very basic allocation before kernel heap's setup, then redirects everything to kheap.c */
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
#include <kmalloc.h>
#include <kheap.h>
#include <paging.h>

// Defined in paging.c
extern page_directory_t *kernel_dir;

// Defined in kheap.c
extern kheap_t *kheap;

// Defined in main.c
extern u32int placement_address;
extern u32int phys_address;

static void *kmalloc_internal(u32int sz, int align, u32int *phys) {
	if (kheap) {
		void *addr = alloc(sz, align, kheap);
		if (phys) {
			page_t *pg = get_page((u32int) addr, 0, kernel_dir);
			*phys = pg->frame * 0x1000 + (u32int)addr % 0x1000;
		}
		return addr;
	}
	if (align) {
		placement_address &= 0xFFFFF000;	// Page align
		phys_address &= 0xFFFFF000;
		placement_address += 0x1000;		// Next page so we don't overwrite something
		phys_address += 0x1000;
	}
	if (phys) {
		*phys = phys_address;			// Physical address
	}
	u32int tmp = placement_address;
	placement_address += sz;
	phys_address += sz;
	return (void *)tmp;
}

void kfree(void *addr) {
	free(addr, kheap);
}

void *kmalloc(u32int sz) { return (void *)kmalloc_internal(sz, 0, NULL); }
void *kmalloc_a(u32int sz) { return (void *)kmalloc_internal(sz, 1, NULL); }
void *kmalloc_p(u32int sz, u32int *phys) { return (void *)kmalloc_internal(sz, 0, phys); }
void *kmalloc_ap(u32int sz, u32int *phys) { return (void *)kmalloc_internal(sz, 1, phys); }
