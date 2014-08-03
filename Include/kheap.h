/* kheap.h - data types and declarations for kernel heap setup and
 * management */

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

#ifndef KHEAP_H
#define KHEAP_H
#include <common.h>
#include <ordered_array.h>

#define KHEAP_START 0xD0000000
#define KHEAP_INIT_SIZE 0x100000
#define KHEAP_INDEX_SIZE 0x20000
#define KHEAP_MAGIC 0xDEADBABA
#define KHEAP_MIN_SIZE 0x70000

typedef struct kheap_header_struct {
	uint32_t magic;
	uint8_t hole;
	uint32_t size;
} kheap_header_t;

typedef struct kheap_footer_struct {
	uint32_t magic;
	kheap_header_t *header;
} kheap_footer_t;

typedef struct kheap_struct {
	ordered_array_t index;
	uint32_t start_address; // Start of allocated space
	uint32_t end_address; // End of allocated space
	uint32_t max_address; // Highest possible address to which heap can expand
	uint8_t supervisor; // Should requested pages be in kernel mode
	uint8_t rw; // Should requested pages be rw
} kheap_t;

kheap_t *create_heap(uint32_t start, uint32_t end, uint32_t max, uint8_t supervisor,
		uint8_t rw);

// Allocates coniguous memory within heap
void *alloc(uint32_t size, uint8_t align, kheap_t *heap);

// Releases block allocated with alloc
void free(void *p, kheap_t *heap);

#endif /* KHEAP_H */
