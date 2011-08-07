/* paging.h - declarations and data types for paging */
/* Copyright (C) 2011 Bth8 <bth8fwd@gmail.com>
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
	u32int present	: 1;	// Present in memory
	u32int rw		: 1;	// Read-write if set
	u32int user		: 1;	// User-accessible
	u32int unused	: 2;
	u32int accessed	: 1;	// Has it been accessed since refresh?
	u32int dirty	: 1;	// Has it been written to since refresh?
	u32int zero		: 1;	
	u32int global	: 1;	// Not updated in TLB upon CR3 refresh
	u32int avail	: 3;	// Available for kernel use
	u32int frame	: 20;	// Frame pointer >> 12
} page_t;

typedef struct page_table {
	page_t pages[1024];
} page_table_t;

typedef struct page_directory_entry {
	u32int present	: 1;	// Present in memory
	u32int rw		: 1;	// Read-write
	u32int user		: 1;	// User-accessible
	u32int unused	: 2;
	u32int accessed	: 1;	// Accessed since last refresh?
	u32int zero		: 1;
	u32int size		: 1;	// 4kb if unset, 4Mb if set
	u32int unused2	: 1;
	u32int avail	: 3;	// Available for kernel use
	u32int table	: 20;	// Table address >> 12
} page_directory_entry_t;

typedef struct page_directory {
	// Physical addresses of tables
	page_directory_entry_t tables_phys[1024];

	page_table_t *tables[1024];

	// Our physical address
	u32int physical_address;
} page_directory_t;

void init_paging(u32int mem_end);
void switch_page_dir(page_directory_t *newdir);
void alloc_frame(page_t *page, int kernel, int rw, int global);
void free_frame(page_t *page);
// Gets specified page from dir. If make, create if not already present
page_t *get_page(u32int address, int make, page_directory_t *dir);
page_directory_t *clone_directory(page_directory_t *src);
void free_dir(page_directory_t *dir);

#endif
