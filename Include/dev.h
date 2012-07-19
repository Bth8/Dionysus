/* dev.h - header for devfs */
/* Copyright (C) 2011, 2012 Bth8 <bth8fwd@gmail.com>
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
#include <ordered_array.h>

#define KERNEL_BLOCKSIZE	512

#define MAJOR(x) ((x >> 24) & 0xFF)
#define MINOR(x) (x & 0xFFFFFF)
#define MKDEV(x, y) (((x & 0xFF) << 24) | (y & 0xFFFFFF))

struct chrdev_driver {
	const char *name;
	struct file_ops ops;
};

struct dev_file {
	fs_node_t node;
	struct dev_file *next;	// Linked list, y'all
};

struct partition {
	u8int boot;
	u8int start_head;		// All of the start/end crap is obsolete
	u32int start_sect:	6;	// Ignore.
	u32int start_cyl:	10;
	u8int sys_id;
	u8int end_head;
	u32int end_sect:	6;
	u32int end_cyl:		10;
	u32int rel_sect;		// This is what we care about
	u32int nsects;
} __attribute__((packed));

struct mbr {
	u8int bootloader[436];
	u8int disk_id[10];
	struct partition partitions[4];
	u8int magic[2];
} __attribute__((packed));

struct blkdev_driver;

struct blockdev {
	struct blkdev_driver *driver;
	u32int minor;
	u32int block_size;
	u32int offset;			// Offset in sectors
	u32int capacity;		// Capacity in sectors
	struct blockdev *next;
};

struct blkdev_driver {
	const char *name;
	u32int (*read)(u32int, u32int, u32int, void*);
	u32int (*write)(u32int, u32int, u32int, const void*);
	s32int (*ioctl)(u32int, u32int, void *);
	u32int nreal;			// Number of real devices
	ordered_array_t devs;	// Block devices managed by this driver
};

void init_devfs(void);
s32int devfs_register(const char *name, u32int flags, u32int major,
					u32int minor, u32int mode, u32int uid, u32int gid);
s32int register_chrdev(u32int major, const char *name, struct file_ops fops);
s32int register_blkdev(u32int major, const char *name,
						u32int (*read)(u32int, u32int, u32int, void*),
						u32int (*write)(u32int, u32int, u32int, const void*),
						s32int (*ioctl)(u32int, u32int, void *),
						u32int nreal, u32int sectsize, u32int *drv_sizes);

#endif
