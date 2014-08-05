/* kmalloc.c - very basic allocation before kernel heap's setup, then
 * redirects everything to kheap.c
 */

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
#include <kmalloc.h>
#include <kheap.h>
#include <paging.h>

// Defined in paging.c
extern page_directory_t *kernel_dir;

// Defined in kheap.c
extern kheap_t *kheap;

// Defined in main.c
extern uint32_t placement_address;
extern uint32_t phys_address;

volatile uint8_t kmalloc_lock = 0;

static void *kmalloc_internal(uint32_t sz, int align, uint32_t *phys) {
	if (kheap) {
		spin_lock(&kmalloc_lock);
		void *addr = alloc(sz, align, kheap);
		if (phys) {
			page_t *pg = get_page((uint32_t) addr, 0, kernel_dir);
			*phys = pg->frame * 0x1000 + (uint32_t)addr % 0x1000;
		}
		spin_unlock(&kmalloc_lock);
		return addr;
	}
	if (align) {
		// Page align
		placement_address &= 0xFFFFF000;
		phys_address &= 0xFFFFF000;
		// Start at next page so we don't overwrite something
		placement_address += 0x1000;
		phys_address += 0x1000;
	}
	if (phys) {
		*phys = phys_address;
	}
	uint32_t tmp = placement_address;
	placement_address += sz;
	phys_address += sz;
	return (void *)tmp;
}

void kfree(void *addr) {
	spin_lock(&kmalloc_lock);
	free(addr, kheap);
	spin_unlock(&kmalloc_lock);
}

void *kmalloc(uint32_t sz) {
	return (void *)kmalloc_internal(sz, 0, NULL);
}

void *kmalloc_a(uint32_t sz) {
	return (void *)kmalloc_internal(sz, 1, NULL);
}

void *kmalloc_p(uint32_t sz, uint32_t *phys) {
	return (void *)kmalloc_internal(sz, 0, phys);
}

void *kmalloc_ap(uint32_t sz, uint32_t *phys) {
	return (void *)kmalloc_internal(sz, 1, phys);
}
