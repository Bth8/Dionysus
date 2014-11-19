/* block.c - registering, managing, and accessing blockdevs */

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
#include <block.h>
#include <vfs.h>
#include <string.h>
#include <printf.h>
#include <paging.h>
#include <structures/list.h>
#include <kmalloc.h>
#include <errno.h>

struct blkdev_driver blk_drivers[256];

void init_blockdev(void) {
	int i;
	for (i = 0; i < 256; i++) {
		blk_drivers[i].name = "Default";
		blk_drivers[i].ops = (struct file_ops){ NULL };
		blk_drivers[i].devs = NULL;
	}
}

struct blkdev_driver *get_blkdev_driver(uint32_t major) {
	if (major <= 0 || major > 256)
		return NULL;

	if (strcmp(blk_drivers[major - 1].name, "Default") == 0)
		return NULL;

	return &blk_drivers[major - 1];
}

spinlock_t block_lock = 0;

int32_t register_blkdev(uint32_t major, const char *name, struct file_ops fops) {
	spin_lock(&block_lock);
	if (major == 0) {
		for (major = 1; strcmp(blk_drivers[major - 1].name, "Default") != 0;
				major++)
			if (major == 256)
				return -1;
	}

	blk_drivers[major - 1].devs = list_create();
	if (!blk_drivers[major - 1].devs) {
		spin_unlock(&block_lock);
		return -1;
	}
	blk_drivers[major - 1].name = name;
	blk_drivers[major - 1].ops = fops;
	spin_unlock(&block_lock);

	printf("Blockdev driver %s added\n", name);
	return major;
}

blkdev_t *get_blkdev(dev_t dev) {
	struct blkdev_driver *driver = get_blkdev_driver(MAJOR(dev));
	if (!driver)
		return NULL;

	blkdev_t *blockdev = NULL;
	node_t *node;
	foreach(node, driver->devs) {
		blockdev = (blkdev_t *)node->data;
		if (MINOR(dev) < blockdev->minor + blockdev->max_part)
			break;
	}

	if (!blockdev)
		return NULL;

	foreach(node, blockdev->partitions) {
		struct part *partition = (struct part *)node->data;
		if (partition->minor == MINOR(dev))
			return blockdev;
	}

	return NULL;

}

blkdev_t *alloc_blkdev(void) {
	blkdev_t *blockdev = (blkdev_t *)kmalloc(sizeof(blkdev_t));
	if (!blockdev)
		return NULL;

	memset(blockdev, 0, sizeof(blkdev_t));

	blockdev->queue = list_create();
	if (!blockdev->queue) {
		kfree(blockdev);
		return NULL;
	}

	blockdev->partitions = list_create();
	if (!blockdev->partitions) {
		list_destroy(blockdev->queue);
		kfree(blockdev);
		return NULL;
	}

	return blockdev;
}

int32_t autopopulate_blkdev(blkdev_t *dev) {
	ASSERT(!dev->partitions);
	// Create the first entry
	struct part *part0 = (struct part *)kmalloc(sizeof(struct part));
	if (!part0)
		return -ENOMEM;

	part0->minor = dev->minor;
	part0->offset = 0;
	part0->size = dev->size;
	node_t *node = list_insert(dev->partitions, part0);
	if (!node) {
		kfree(part0);
		return -ENOMEM;
	}

	struct mbr *mbr = (struct mbr *)kmemalign(KERNEL_BLOCKSIZE, sizeof(struct mbr));
	if (!mbr)
		goto nomem;

	bio_t *bio = (bio_t *)kmalloc(sizeof(bio_t));
	if (!bio) {
		kfree(mbr);
		goto nomem;
	}

	bio->page = (resolve_physical((uintptr_t)mbr) / PAGE_SIZE) * PAGE_SIZE;
	bio->page = resolve_physical((uintptr_t)mbr) % PAGE_SIZE;
	bio->nsectors = 1;

	make_request_blkdev(MKDEV(dev->major, dev->minor), 0, bio, 0);

	if (mbr->magic[0] != 0x55 || mbr->magic[1] != 0xAA) {
		printf("Warning: Invalid partition table\nDevice: %u\n",
			MKDEV(dev->major, dev->minor));
		kfree(mbr);
		return 0;
	}

	uint32_t i;
	for (i = 0; i < 4; i++) {
		if (mbr->partitions[i].sys_id != 0) {
			struct part *partition = (struct part *)kmalloc(sizeof(struct part));
			if (!partition) {
				kfree(partition);
				kfree(mbr);
				goto nomem;
			}
			partition->minor = dev->minor + i + 1;
			partition->offset = mbr->partitions[i].rel_sect;
			partition->size = mbr->partitions[i].nsects;
			node = list_insert(dev->partitions, partition);
			if (!node) {
				kfree(partition);
				kfree(mbr);
				goto nomem;
			}
		}
	}

	return 0;

nomem:
	foreach(node, dev->partitions)
		list_remove(dev->partitions, node);
	return -ENOMEM;
}

void free_blkdev(blkdev_t *dev) {
	ASSERT(dev);

	list_destroy(dev->partitions);
	list_destroy(dev->queue);
	kfree(dev);
}

int32_t add_blkdev(blkdev_t *dev) {
	ASSERT(dev);

	struct blkdev_driver *driver = get_blkdev_driver(dev->major);
	if (!driver)
		return -ESRCH;

	blkdev_t *blockdev = NULL;
	node_t *node;
	foreach(node, driver->devs) {
		blockdev = (blkdev_t *)node->data;
		if (dev->minor > blockdev->minor &&
				dev->minor < blockdev->minor + blockdev->max_part)
			return -EEXIST;
		if (dev->minor + dev->max_part > blockdev->minor &&
				dev->minor + dev->max_part < blockdev->minor + blockdev->max_part)
			return -EEXIST;
		if (blockdev->minor > dev->minor)
			break;
	}

	if (node)
		node = list_insert_before(driver->devs, node, dev);
	else
		node = list_insert(driver->devs, dev);

	if (!node)
		return -ENOMEM;

	return 0;
}

static void collate_requests(list_t *queue) {
	node_t *node;
	node_t *prev = NULL;
	foreach(node, queue) {
		if (!prev) {
			prev = node;
			continue;
		}
		request_t *prev_req = (request_t *)prev->data;
		request_t *req = (request_t *)node->data;
		if (prev_req->first_sector + prev_req->nsectors != req->first_sector) {
			prev = node;
			continue;
		}
		if (prev_req->flags != req->flags) {
			prev = node;
			continue;
		}
		prev_req->nsectors += req->nsectors;
		list_merge(prev_req->bios, req->bios);
		list_destroy(req->bios);
		list_remove(queue, node);
		node = prev;
	}
}

int32_t make_request_blkdev(dev_t dev, uint32_t first_sector, bio_t *bios,
		uint32_t write) {
	if (!bios)
		return -EFAULT;

	bio_t *bio_iter = bios;

	blkdev_t *blockdev = get_blkdev(dev);
	if (!blockdev)
		return -EINVAL;

	if (!blockdev->handler)
		return -EINVAL;

	spin_lock(&blockdev->lock);

	struct part *partition;
	node_t *node = NULL;
	foreach(node, blockdev->partitions) {
		partition = (struct part *)node->data;
		if (partition->minor == MINOR(dev))
			break;
	}

	if (!node) {
		spin_unlock(&blockdev->lock);
		return -EINVAL;
	}

	while (bio_iter) {

		if (first_sector > partition->size)
			break;

		request_t *request = (request_t *)kmalloc(sizeof(request_t));
		if (!request)
			break;

		request->flags = write ? BLOCK_DIR_WRITE : BLOCK_DIR_READ;
		request->first_sector = first_sector + partition->offset;
		request->nsectors = 0;
		request->bios = list_create();
		if (!request->bios) {
			kfree(request);
			break;
		}

		for (; bio_iter; bio_iter = bio_iter->next) {
			if ( first_sector + request->nsectors + bio_iter->nsectors >
					partition->size) {
				first_sector += request->nsectors;
				break;
			}
			request->nsectors += bio_iter->nsectors;
			list_insert(request->bios, bio_iter);
		}

		node_t *node;
		foreach(node, blockdev->queue) {
			request_t *req = (request_t *)node->data;
			if (req->first_sector > request->first_sector)
				break;
		}
		if (node)
			node = list_insert_before(blockdev->queue, node, request);
		else
			node = list_insert(blockdev->queue, request);

		if (!node) {
			kfree(request);
			break;
		}
	}

	spin_unlock(&blockdev->lock);

	collate_requests(blockdev->queue);

	blockdev->handler(blockdev);

	return 0;
}
