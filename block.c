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
#include <structures/mutex.h>

struct blkdev_driver blk_drivers[256];

void init_blockdev(void) {
	int i;
	for (i = 0; i < 256; i++) {
		blk_drivers[i].name = "Default";
		blk_drivers[i].ops = (struct file_ops){ NULL };
		blk_drivers[i].devs = NULL;
	}
}

struct blkdev_driver *get_blkdev_driver(dev_t major) {
	if (major <= 0 || major > 256)
		return NULL;

	if (strcmp(blk_drivers[major - 1].name, "Default") == 0)
		return NULL;

	return &blk_drivers[major - 1];
}

spinlock_t block_lock = 0;

int32_t register_blkdev(dev_t major, const char *name, struct file_ops fops) {
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

size_t get_block_size(dev_t dev) {
	blkdev_t *blockdev = get_blkdev(dev);
	if (!blockdev)
		return 0;

	return blockdev->sector_size;
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

	blockdev->mutex = create_mutex(0);
	if (!blockdev->mutex) {
		list_destroy(blockdev->partitions);
		list_destroy(blockdev->queue);
		kfree(blockdev);
		return NULL;
	}

	return blockdev;
}

int32_t autopopulate_blkdev(blkdev_t *dev) {
	ASSERT(!dev->partitions->head);

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

	size_t mbr_size = sizeof(struct mbr) / dev->sector_size;
	if (sizeof(struct mbr) % dev->sector_size)
		mbr_size++;
	mbr_size *= dev->sector_size;

	int32_t ret = 0;
	struct mbr *mbr = (struct mbr *)kmalloc(mbr_size);
	if (!mbr) {
		ret = -ENOMEM;
		goto fail;
	}

	/* As the device shouldn't be in the driver's list yet,
	 * we'll have to construct our request manually
	 */

	request_t *req = (request_t *)kmalloc(sizeof(request_t));
	if (!req) {
		kfree(mbr);
		ret = -ENOMEM;
		goto fail;
	}

	req->flags = 0;
	req->first_sector = 0;
	req->nsectors = 0;
	req->status = BLOCK_REQ_UNSCHED;
	req->rc = 0;
	req->dev = dev;
	req->bios = list_create();
	if (!req->bios) {
		kfree(req);
		kfree(mbr);
		ret = -ENOMEM;
		goto fail;
	}
	req->wq = create_waitqueue();
	if (!req->wq) {
		list_destroy(req->bios);
		kfree(req);
		kfree(mbr);
		ret = -ENOMEM;
		goto fail;
	}

	bio_t *bio = (bio_t *)kmalloc(sizeof(bio_t));
	if (!bio) {
		kfree(mbr);
		free_request(req);
		ret = -ENOMEM;
		goto fail;
	}

	bio->page = resolve_physical((uintptr_t)mbr);
	bio->offset = bio->page % PAGE_SIZE;
	bio->page = (bio->page / PAGE_SIZE) * PAGE_SIZE;
	bio->nsectors = mbr_size / dev->sector_size;

	if ((ret = add_bio_to_request_blkdev(req, bio)) < 0) {
		kfree(mbr);
		free_request(req);
		kfree(bio);
		goto fail;
	}

	ret = post_and_wait_blkdev(req);
	free_request(req);

	if (ret < 0) {
		kfree(mbr);
		goto fail;
	}

	if (mbr->magic[0] != 0x55 || mbr->magic[1] != 0xAA) {
		printf("Warning: Invalid partition table device %d %d\n",
			dev->major, dev->minor);
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
				ret = -ENOMEM;
				goto fail;
			}
			partition->minor = dev->minor + i + 1;
			partition->offset = mbr->partitions[i].rel_sect;
			partition->size = mbr->partitions[i].nsects;
			node = list_insert(dev->partitions, partition);
			if (!node) {
				kfree(partition);
				kfree(mbr);
				ret = -ENOMEM;
				goto fail;
			}
		}
	}

	return 0;

fail:
	foreach(node, dev->partitions)
		list_remove(dev->partitions, node);
	return ret;
}

void free_blkdev(blkdev_t *dev) {
	ASSERT(dev);

	list_destroy(dev->partitions);
	list_destroy(dev->queue);
	destroy_mutex(dev->mutex);

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

node_t *next_ready_request_blkdev(blkdev_t *dev) {
	acquire_mutex(dev->mutex);

	node_t *node = NULL;
	while (1) {
		node = dev->queue->head;
		if (!node)
			break;

		request_t *req = (request_t *)node->data;

		if (req->status == BLOCK_REQ_INTR) {
			list_dequeue(dev->queue, node);
			kfree(node);
			wake_queue(req->wq);
			continue;
		}

		if (req->status == BLOCK_REQ_PENDING) {
			req->status = BLOCK_REQ_RUNNING;
			break;
		}
	}

	release_mutex(dev->mutex);

	return node;
}

request_t *create_request_blkdev(dev_t dev, uint32_t first_sector,
		uint32_t flags) {
	blkdev_t *device = get_blkdev(dev);
	if (!device)
		return NULL;

	struct part *partition = NULL;
	node_t *node;
	foreach(node, device->partitions) {
		partition = (struct part *)node->data;
		if (partition->minor == MINOR(dev))
			break;
	}

	if (first_sector > partition->size)
		return NULL;

	request_t *req = (request_t *)kmalloc(sizeof(request_t));
	if (!req)
		return NULL;

	req->flags = flags;
	req->first_sector = first_sector + partition->offset;
	req->nsectors = 0;
	req->status = BLOCK_REQ_UNSCHED;
	req->rc = 0;
	req->dev = device;
	req->bios = list_create();
	if (!req->bios) {
		kfree(req);
		return NULL;
	}

	req->wq = create_waitqueue();
	if (!req->wq) {
		kfree(req->bios);
		kfree(req);
		return NULL;
	}

	return req;
}

int32_t add_bio_to_request_blkdev(request_t *req, bio_t *bio) {
	ASSERT(req->status == BLOCK_REQ_UNSCHED);

	node_t *node = list_insert(req->bios, bio);
	if (!node)
		return -ENOMEM;

	req->nsectors += bio->nsectors;
	return 0;
}

int32_t post_request_blkdev(request_t *req) {
	ASSERT(req->status == BLOCK_REQ_UNSCHED);

	acquire_mutex(req->dev->mutex);

	req->status = BLOCK_REQ_PENDING;
	node_t *node = list_insert(req->dev->queue, req);

	release_mutex(req->dev->mutex);

	if (!node)
		return -ENOMEM;

	return req->dev->handler(req->dev);
}

int32_t wait_request_blkdev(request_t *req) {
	uint32_t interrupted = 0;

	do {
		if (req->status == BLOCK_REQ_FINISHED)
			break;

		interrupted = sleep_thread(req->wq, SLEEP_INTERRUPTABLE);
	} while (interrupted == 0);

	if (interrupted) {
		req->status = BLOCK_REQ_INTR;
		return -EINTR;
	}

	return req->rc;
}

int32_t post_and_wait_blkdev(request_t *req) {
	int32_t ret = post_request_blkdev(req);
	if (ret < 0)
		return ret;

	return wait_request_blkdev(req);
}

int end_request(request_t *req, int32_t error, uint32_t nsectors) {
	if (error < 0)
		nsectors = req->nsectors;

	req->nsectors -= nsectors;
	req->first_sector += nsectors;

	while (1) {
		node_t *node = req->bios->head;
		if (!node) {
			ASSERT(nsectors == 0);
			break;
		}

		bio_t *bio = (bio_t *)node->data;
		if (bio->nsectors <= nsectors) {
			nsectors -= bio->nsectors;
			list_remove(req->bios, node);
			continue;
		}

		bio->offset += nsectors * req->dev->sector_size;
		bio->nsectors -= nsectors;
		break;
	}

	if (req->status == BLOCK_REQ_INTR) {
		wake_queue(req->wq);
		return 0;
	}

	if (req->nsectors == 0) {
		req->status = BLOCK_REQ_FINISHED;
		wake_queue(req->wq);
		return 0;
	}

	return 1;
}

void free_request(request_t *req) {
	if (!req)
		return;

	ASSERT(req->status == BLOCK_REQ_UNSCHED ||
		req->status == BLOCK_REQ_INTR ||
		req->status == BLOCK_REQ_FINISHED);

	destroy_waitqueue(req->wq);
	list_destroy(req->bios);
	kfree(req);
}
