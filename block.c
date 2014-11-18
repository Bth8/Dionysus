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

	uint32_t i;
	for (i = 0; i < blockdev->nreal; i++)
		if (blockdev->partitions[i].minor == MINOR(dev))
			return blockdev;

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

	return blockdev;
}

int32_t autopopulate_blkdev(blkdev_t *dev);

void free_blkdev(blkdev_t *dev) {
	ASSERT(dev);

	kfree(dev->partitions);
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

	blkdev_t *blockdev = get_blkdev(dev);
	if (!blockdev)
		return -EINVAL;

	if (!blockdev->handler)
		return -EINVAL;

	spin_lock(&blockdev->lock);

	struct part *partition = NULL;
	uint32_t i;
	for (i = 0; i < blockdev->nreal; i++) {
		if (blockdev->partitions[i].minor == MINOR(dev)) {
			partition = &blockdev->partitions[i];
			break;
		}
	}

	if (!partition) {
		spin_unlock(&blockdev->lock);
		return -EINVAL;
	}

	if (first_sector > partition->size){
		spin_unlock(&blockdev->lock);
		return -EFAULT;
	}

	request_t *request = (request_t *)kmalloc(sizeof(request_t));
	if (!request) {
		spin_unlock(&blockdev->lock);
		return -ENOMEM;
	}

	request->flags = write ? BLOCK_DIR_WRITE : BLOCK_DIR_READ;
	request->first_sector = first_sector + partition->offset;
	request->nsectors = 0;
	request->bios = list_create();
	if (!request->bios) {
		spin_unlock(&blockdev->lock);
		kfree(request);
		return -EINVAL;
	}

	bio_t *bio;
	for (bio = bios; bio; bio = bio->next) {
		list_insert(request->bios, bio);
		request->nsectors += bio->nsectors;
	}

	if (first_sector + request->nsectors > partition->size) {
		spin_unlock(&blockdev->lock);
		kfree(request);
		return -EINVAL;
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
		spin_unlock(&blockdev->lock);
		kfree(request);
		return -ENOMEM;
	}

	spin_unlock(&blockdev->lock);

	collate_requests(blockdev->queue);

	blockdev->handler(blockdev);

	return 0;
}
