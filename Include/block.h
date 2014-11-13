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

#define KERNEL_BLOCKSIZE	512

typedef struct blockdev blkdev_t;
typedef void (*request_handler_t)(blkdev_t*);

typedef struct bio {
	uintptr_t page;
	off_t offset;
	uint32_t nsectors;
	struct bio *next;
} bio_t;

typedef struct {
	uint32_t flags;
	uint32_t first_sector;
	uint32_t nsectors;
	bio_t *bios;
} request_t;

struct part {
	uint32_t minor;
	uint32_t offset;	// in 512B sectors
	uint32_t size;
};

typedef struct blockdev {
	uint32_t major;
	uint32_t minor;		// First minor
	uint32_t max_part;	// Maximum possible partitions
	uint32_t nreal;		// Actual number of minors
	struct part *partitions;
	size_t sector_size;
	spinlock_t lock;
	request_handler_t handler;
	list_t *queue;
	void *private_data;
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
blkdev_t *alloc_blkdev(struct blkdev_driver *driver);
int32_t autopopulate_blkdev(blkdev_t *dev);
void free_blkdev(blkdev_t *dev);
int32_t add_blkdev(blkdev_t *dev);
blkdev_t *get_blkdev(dev_t dev);
int32_t make_request_blkdev(dev_t dev, uint32_t first_sector,
	uint32_t nsectors, bio_t *bios);

#endif /* BLOCK_H */