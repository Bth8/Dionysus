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

	blk_drivers[major - 1].name = name;
	blk_drivers[major - 1].ops = fops;
	blk_drivers[major - 1].devs = NULL;
	spin_unlock(&block_lock);

	printf("Blockdev driver %s added\n", name);
	return major;
}

blkdev_t *get_blkdev(dev_t dev) {
	blkdev_t *blockdev = blk_drivers[MAJOR(dev) - 1].devs;
	while (blockdev->minor + blockdev->max_part <= dev) {
		blockdev = blockdev->next;
		if (!blockdev)
			return NULL;
	}

	uint32_t i;
	for (i = 0; i < blockdev->nreal; i++)
		if (blockdev->partitions[i].minor == MINOR(dev))
			return blockdev;

	return NULL;

}