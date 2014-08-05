/* paging.h - declarations and data types for paging */

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

#ifndef PAGING_H
#define PAGING_H
#include <common.h>

typedef struct page {
	uint32_t present	: 1;	// Present in memory
	uint32_t rw		: 1;	// Read-write if set
	uint32_t user		: 1;	// User-accessible
	uint32_t unused	: 2;
	uint32_t accessed	: 1;	// Has it been accessed since refresh?
	uint32_t dirty	: 1;	// Has it been written to since refresh?
	uint32_t zero		: 1;
	uint32_t global	: 1;	// Not updated in TLB upon CR3 refresh
	uint32_t avail	: 3;	// Available for kernel use
	uint32_t frame	: 20;	// Frame pointer >> 12
} page_t;

typedef struct page_table {
	page_t pages[1024];
} page_table_t;

typedef struct page_directory_entry {
	uint32_t present	: 1;	// Present in memory
	uint32_t rw		: 1;	// Read-write
	uint32_t user		: 1;	// User-accessible
	uint32_t unused	: 2;
	uint32_t accessed	: 1;	// Accessed since last refresh?
	uint32_t dirty	: 1;	// Written to since last refresh (4Mb only)
	uint32_t size		: 1;	// 4kB if unset, 4MB if set
	uint32_t global	: 1;	// Global (4MB only)
	uint32_t avail	: 3;	// Available for kernel use
	uint32_t table	: 20;	// Table address >> 12
} page_directory_entry_t;

typedef struct page_directory {
	// Physical addresses of tables
	page_directory_entry_t tables_phys[1024];

	page_table_t *tables[1024];

	// Our physical address
	uint32_t physical_address;
} page_directory_t;

void init_paging(uint32_t memlength);
void switch_page_dir(page_directory_t *newdir);
void global_flush(void);
void alloc_frame(page_t *page, int kernel, int rw, int global);
void free_frame(page_t *page);
// Gets specified page from dir. If make, create if not already present
page_t *get_page(uint32_t address, int make, page_directory_t *dir);
page_directory_t *clone_directory(page_directory_t *src);
void free_dir(page_directory_t *dir);

#endif /* PAGING_H */
