/* block.h - blockdev driver management */

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

#ifndef BLOCK_H
#define BLOCK_H

#include <common.h>
#include <vfs.h>
#include <structures/list.h>

typedef struct request_queue request_queue_t;

typedef void (*request_handler_t)(request_queue_t*);

typedef struct {
	uintptr_t page;
	off_t offset;
	size_t nsectors;
} bio_t;

typedef struct {
	uint32_t flags;
	off_t first_sector;
	size_t nsectors;
	list_t *bios;
} request_t;

struct request_queue {
	spinlock_t lock;
	request_handler_t handler;
	list_t *requests;
};

struct part {
	uint32_t minor;
	off_t offset;
	size_t size;
};

typedef struct blockdev {
	struct blkdev_driver *driver;
	uint32_t minor;		// First minor
	uint32_t nreal;		// Actual number of minors
	struct part *partitions;
	size_t sector_size;
	struct request_queue *queue;
	struct blockdev *next;
} blkdev_t;

struct blkdev_driver {
	const char *name;
	struct file_ops ops;
	blkdev_t *devs; // Block devices managed by this driver
};

void init_blockdev(void);
struct blkdev_driver *get_blkdev_driver(uint32_t major);
int32_t register_blkdev(uint32_t major, const char *name, struct file_ops fops);
request_queue_t *blk_create_queue(request_handler_t handler);

#endif /* BLOCK_H */