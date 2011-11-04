/* dev.h - header for devfs */
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

#ifndef DEV_H
#define DEV_H
#include <common.h>
#include <vfs.h>

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

void devfs_init(void);
u32int devfs_register(const char *name, u32int flags, u32int major,
					u32int minor, u32int mode, u32int uid, u32int gid);
u32int register_chrdev(u32int major, const char *name, struct file_ops fops);

#endif
