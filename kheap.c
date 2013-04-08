/* kheap.c - sets up and manages kernel heap */
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
#include <kheap.h>
#include <kmalloc.h>
#include <ordered_array.h>
#include <paging.h>

// Defined in paging.c
extern page_directory_t *current_dir;

kheap_t *kheap = NULL;

static s32int find_smallest_hole(size_t size, u8int align, kheap_t *heap) {
	u32int i;
	for (i = 0; i < heap->index.size; i++) {
		kheap_header_t *header = (kheap_header_t *)lookup_ordered_array(i, &heap->index);

		if (align) {
			// Page align the starting point
			u32int location = (u32int)header;
			s32int offset = 0;
			if ((location + sizeof(kheap_header_t)) & 0xFFFFF000)
				offset = 0x1000 - ((location + sizeof(kheap_header_t)) % 0x1000);
			s32int hole_size = (s32int) header->size - offset;
			// Do we fit?
			if (hole_size >= (s32int)size)
				break;
		} else if (header->size > size)
			break;
	}
	if (i == heap->index.size)
		return -1;
	return i;
}

static u32int kheap_header_less_than(type_t a, type_t b) {
	return (((kheap_header_t *)a)->size < ((kheap_header_t *)b)->size)?1:0;
}

kheap_t *create_heap(u32int start, u32int end, u32int max, u8int supervisor, u8int rw) {
	kheap_t *heap = (kheap_t *)kmalloc(sizeof(kheap_t));
	// Assert we're page-aligned
	ASSERT(!(start % 0x1000));
	ASSERT(!(end % 0x1000));

	// Sanity check
	ASSERT(start <= end && end <= max);

	// Init the index
	heap->index = place_ordered_array((void *)start, KHEAP_INDEX_SIZE, &kheap_header_less_than);

	// Shift start address to where we can actually start allocating
	start += sizeof(type_t) * KHEAP_INDEX_SIZE;

	// Make sure start is page-aligned
	if (start % 0x1000) {
		start &= 0xFFFFF000;
		start += 0x1000;
	}
	// Write everything to the heap structure
	heap->start_address = start;
	heap->end_address = end;
	heap->max_address = max;
	heap->supervisor = supervisor;
	heap->rw = rw;

	// Start off with one large hole in index
	kheap_header_t *hole = (kheap_header_t *) start;
	hole->size = end-start;
	hole->magic = KHEAP_MAGIC;
	hole->hole = 1;
	insert_ordered_array((type_t)hole, &heap->index);

	return heap;
}

static void expand(u32int new_size, kheap_t *heap) {
	// Sanity check
	ASSERT(new_size > heap->end_address - heap->start_address);
	// Get nearest following page boudary
	if (new_size % 0x1000) {
		new_size &= 0xFFFFF000;
		new_size += 0x1000;
	}

	// Assert we stay within the bounds of the heap
	ASSERT(heap->start_address + new_size <= heap->max_address);
	u32int i;
	for (i = heap->end_address - heap->start_address; i < new_size; i += 0x1000)
		alloc_frame(get_page(heap->start_address + i, 1, current_dir),
					(heap->supervisor) ? 1 : 0, (heap->rw) ? 1 : 0, 1);
	heap->end_address = heap->start_address+new_size;
}

static u32int contract(u32int new_size, kheap_t *heap) {
	// Sanity check
	ASSERT(new_size < heap->end_address - heap->start_address);
	// Nearest following page boundary
	if (new_size % 0x1000) {
		new_size &= 0xFFFFF000;
		new_size += 0x1000;
	}
	// Don't contract too far
	if (new_size < KHEAP_MIN_SIZE)
		new_size = KHEAP_MIN_SIZE;
	u32int i;
	for (i = heap->end_address - heap->start_address - 0x1000; new_size < 1; i -= 0x1000)
		free_frame(get_page(heap->start_address + i, 0, current_dir));
	heap->end_address = heap->start_address + i;
	return new_size;
}

void *alloc(u32int size, u8int align, kheap_t *heap) {
	u32int new_size = size + sizeof(kheap_header_t) + sizeof(kheap_footer_t);
	s32int i = find_smallest_hole(new_size, align, heap);
	if (i < 0) { // No suitable hole
		u32int old_length = heap->end_address - heap->start_address;
		u32int old_end_address = heap->end_address;
		// We need to allocate more space
		expand(old_length + new_size, heap);
		u32int new_length = heap->end_address - heap->start_address;
		// Find last header by location
		s32int idx = -1;
		u32int val = 0x0;
		for (i = 0; (u32int)i < heap->index.size; i++) {
			u32int tmp = (u32int)lookup_ordered_array(i, &heap->index);
			if (tmp > val) {
				val = tmp;
				idx = i;
			}
		}
		// No headers. We must add a new one
		if (idx == -1) {
			kheap_header_t *header = (kheap_header_t *)old_end_address;
			header->magic = KHEAP_MAGIC;
			header->size = new_length - old_length;
			header->hole = 1;
			kheap_footer_t *footer = (kheap_footer_t *)(old_end_address + header->size - sizeof(kheap_footer_t));
			footer->magic = KHEAP_MAGIC;
			footer->header = header;
			insert_ordered_array((type_t)header, &heap->index);
		} else {
			// Adjust last header
			kheap_header_t *header = lookup_ordered_array(idx, &heap->index);
			header->size += new_length - old_length;
			// And rewrite the footer
			kheap_footer_t *footer = (kheap_footer_t *)((u32int)header + header->size - sizeof(kheap_footer_t));
			footer->magic = KHEAP_MAGIC;
			footer->header = header;
		}
		// There's enough space now. Recurse
		return alloc(size, align, heap);
	}
	kheap_header_t *orig_hole_header = (kheap_header_t *)lookup_ordered_array(i, &heap->index);
	u32int orig_hole_pos = (u32int)orig_hole_header;
	u32int orig_hole_size = orig_hole_header->size;
	// Should we split the hole? Is the original hole size - the requested hole size less than the header/footer overhead?
	if (orig_hole_size - new_size < sizeof(kheap_header_t) + sizeof(kheap_footer_t)) {
		// Just increase requested size
		size += orig_hole_size - new_size;
		new_size = orig_hole_size;
	}
	// If we need to page align, do so now and make a new hole in front of our block
	if (align && (orig_hole_pos + sizeof(kheap_header_t)) % 0x1000) {
		u32int new_location = (orig_hole_pos & 0xFFFFF000) + 0x1000 - sizeof(kheap_header_t);
		kheap_header_t *hole_header = (kheap_header_t *)orig_hole_pos;
		hole_header->size = new_location - orig_hole_pos;
		hole_header->magic = KHEAP_MAGIC;
		hole_header->hole = 1;
		kheap_footer_t *hole_footer = (kheap_footer_t *)((u32int)new_location - sizeof(kheap_footer_t));
		hole_footer->magic = KHEAP_MAGIC;
		hole_footer->header = hole_header;
		orig_hole_pos = new_location;
		orig_hole_size -= hole_header->size;
	} else	// We don't need this hole any longer. We can remove it
		remove_ordered_array(i, &heap->index);
	// Overwrite original header
	kheap_header_t *block_header = (kheap_header_t *)orig_hole_pos;
	block_header->magic = KHEAP_MAGIC;
	block_header->hole = 0;
	block_header->size = new_size;
	// And the footer
	kheap_footer_t *block_footer = (kheap_footer_t *)(orig_hole_pos + sizeof(kheap_header_t) + size);
	block_footer->magic = KHEAP_MAGIC;
	block_footer->header = block_header;

	// Do we need to make a new hole after this one?
	if (orig_hole_size - new_size > 0) {
		kheap_header_t *hole_header = (kheap_header_t *)(orig_hole_pos + new_size);
		hole_header->magic = KHEAP_MAGIC;
		hole_header->hole = 1;
		hole_header->size = orig_hole_size - new_size;
		kheap_footer_t *hole_footer = (kheap_footer_t *)((u32int)hole_header + orig_hole_size - new_size - sizeof(kheap_footer_t));
		if ((u32int)hole_footer < heap->end_address) {
			hole_footer->magic = KHEAP_MAGIC;
			hole_footer->header = hole_header;
		}
		// Put new hole in index
		insert_ordered_array((type_t)hole_header, &heap->index);
	}
	// We're finally done
	return (void *)((u32int)block_header + sizeof(kheap_header_t));
}

void free(void *p, kheap_t *heap) {
	// Graceful exit for null pointers
	if (p == NULL)
		return;

	kheap_header_t *header = (kheap_header_t *)((u32int)p - sizeof(kheap_header_t));
	kheap_footer_t *footer = (kheap_footer_t *)((u32int)header + header->size - sizeof(kheap_footer_t));

	// Sanity check
	ASSERT(header->magic == KHEAP_MAGIC);
	ASSERT(footer->magic == KHEAP_MAGIC);

	header->hole = 1;
	// Should we add this to the holes index?
	u32int add = 1;

	// Unify left if thing immediately to the left is a hole footer
	kheap_footer_t *test_footer = (kheap_footer_t *)((u32int)header - sizeof(kheap_footer_t));
	if (test_footer->magic == KHEAP_MAGIC && test_footer->header->hole) {
		u32int cache_size = header->size;
		header = test_footer->header;
		footer->header = header;
		header->size += cache_size;
		add = 0;							// The hole is already in the index, no need to add it
	}
	// Unify right if thing immediately to the right is a hole header
	kheap_header_t *test_header = (kheap_header_t *)((u32int)footer + sizeof(kheap_footer_t));
	if (test_header->magic == KHEAP_MAGIC && test_header->hole) {
		header->size += test_header->size;
		test_footer = (kheap_footer_t *)((u32int)header + header->size - sizeof(kheap_footer_t));
		footer = test_footer;
		u32int i;
		for (i = 0; i < heap->index.size && lookup_ordered_array(i, &heap->index) != (type_t)test_header; i++) {}

		// Make sure we actually found it
		ASSERT(i < heap->index.size);

		// Remove it
		remove_ordered_array(i, &heap->index);
	}

	// If the footer is at the end address, we can contract the heap
	if ((u32int)footer + sizeof(kheap_footer_t) == heap->end_address) {
		u32int old_length = heap->end_address - heap->start_address;
		u32int new_length = contract((u32int)header - heap->start_address, heap);
		// Do we still exist?
		if (header->size - (old_length - new_length) > 0) {
			// Yes. Resize
			header->size -= old_length - new_length;
			footer = (kheap_footer_t *)((u32int)header + header->size - sizeof(kheap_footer_t));
			footer->magic = KHEAP_MAGIC;
			footer->header = header;
		} else {
			u32int i;
			// Nope. Remove ourselves from the index
			for (i = 0; i < heap->index.size && lookup_ordered_array(i, &heap->index) != (type_t)test_header; i++) {}
			if (i < heap->index.size)
				remove_ordered_array(i, &heap->index);
		}
	}
	if (add)
		insert_ordered_array((type_t)header, &heap->index);
}
