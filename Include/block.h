/* block.h - blockdev driver management */

/* Copyright (C) 2015 Bth8 <bth8fwd@gmail.com>
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
#include <structures/mutex.h>

#define BLOCK_REQ_UNSCHED	0
#define BLOCK_REQ_PENDING	1
#define BLOCK_REQ_RUNNING	2
#define BLOCK_REQ_FINISHED	3
#define BLOCK_REQ_INTR		4

#define BLOCK_DIR_WRITE		0x01
#define BLOCK_SYNC			0x02

#define BLOCK_REQ_RESULT(r) r->rc

typedef struct blockdev blkdev_t;
typedef int32_t (*request_handler_t)(blkdev_t*);

typedef struct bio {
	uintptr_t page;
	uintptr_t offset;
	uint32_t nsectors;
} bio_t;

typedef struct {
	uint32_t flags;
	uint32_t first_sector;
	uint32_t nsectors;
	int32_t status;
	uint32_t rc;
	blkdev_t *dev;
	list_t *bios;
	waitqueue_t *wq;
} request_t;

struct part {
	dev_t minor;
	uint32_t offset;	// in sectors
	uint32_t size;
};

typedef struct blockdev {
	dev_t major;
	dev_t minor;		// First minor
	dev_t max_part;	// Maximum possible partitions
	list_t *partitions;
	size_t sector_size;
	uint32_t size;		// Size in sectors
	mutex_t *mutex;
	request_handler_t handler;
	list_t *queue;
	void *private_data;
} blkdev_t;

struct blkdev_driver {
	const char *name;
	struct file_ops ops;
	list_t *devs; // Block devices managed by this driver
};

struct partition_generic {
	uint8_t boot;
	uint8_t start_head; 	// All of the start/end crap is obsolete
	uint32_t start_sect:	6; // Ignore.
	uint32_t start_cyl:		10;
	uint8_t sys_id;
	uint8_t end_head;
	uint32_t end_sect:		6;
	uint32_t end_cyl:		10;
	uint32_t rel_sect;		// This is what we care about
	uint32_t nsects;
} __attribute__((packed));

struct mbr {
	uint8_t bootloader[436];
	uint8_t disk_id[10];
	struct partition_generic partitions[4];
	uint8_t magic[2];
} __attribute__((packed));

void init_blockdev(void);
struct blkdev_driver *get_blkdev_driver(uint32_t major);
int32_t register_blkdev(uint32_t major, const char *name, struct file_ops fops);
blkdev_t *alloc_blkdev(void);
int32_t autopopulate_blkdev(blkdev_t *dev);
void free_blkdev(blkdev_t *dev);
int32_t add_blkdev(blkdev_t *dev);
blkdev_t *get_blkdev(dev_t dev);
size_t get_block_size(dev_t dev);
node_t *next_ready_request_blkdev(blkdev_t *dev);
request_t *create_request_blkdev(dev_t dev, uint32_t first_sector,
	uint32_t flags);
int32_t add_bio_to_request_blkdev(request_t *req, bio_t *bio);
int32_t post_request_blkdev(request_t *req);
int32_t wait_request_blkdev(request_t *req);
int32_t post_and_wait_blkdev(request_t *req);
int end_request(request_t *req, int32_t error, uint32_t nsectors);
void free_request(request_t *req);

#endif /* BLOCK_H */