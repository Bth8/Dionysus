/* dev.h - header for devfs */

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

#ifndef DEV_H
#define DEV_H

#include <common.h>
#include <vfs.h>

#define KERNEL_BLOCKSIZE	512

#define MAJOR(x) ((x >> 24) & 0xFF)
#define MINOR(x) (x & 0xFFFFFF)
#define MKDEV(x, y) (((x & 0xFF) << 24) | (y & 0xFFFFFF))

struct dev_file {
	fs_node_t node;
	struct dev_file *next;
};

void init_devfs(void);

/*
struct partition {
	uint8_t boot;
	uint8_t start_head; // All of the start/end crap is obsolete
	uint32_t start_sect:	6; // Ignore.
	uint32_t start_cyl:	10;
	uint8_t sys_id;
	uint8_t end_head;
	uint32_t end_sect:	6;
	uint32_t end_cyl:		10;
	uint32_t rel_sect; // This is what we care about
	uint32_t nsects;
} __attribute__((packed));

struct mbr {
	uint8_t bootloader[436];
	uint8_t disk_id[10];
	struct partition partitions[4];
	uint8_t magic[2];
} __attribute__((packed));

struct blkdev_driver;

dev_t get_dev(fs_node_t *dev);
int32_t devfs_register(const char *name, uint32_t flags, uint32_t major,
		uint32_t minor, uint32_t mode, uint32_t uid, uint32_t gid);
*/


#endif /* DEV_H */