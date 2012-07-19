/* rootfs.c - Quick-n-dirty fs for setting up sone basic mountpoints */
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

#include <fs/rootfs.h>
#include <common.h>
#include <vfs.h>
#include <string.h>
#include <kmalloc.h>

fs_node_t rootfs_root;
char *names[] = {"real", "dev"};
fs_node_t subdirs[sizeof(names)/sizeof(char *)];

static struct superblock *return_sb(u32int flags, fs_node_t *dev);
struct file_system_type rootfs = {
	.name = "rootfs",
	.flags = FS_NODEV,
	.get_super = return_sb,
};

struct superblock rootfs_sb = {
	.root = &rootfs_root
};

static struct dirent *readdir(fs_node_t *node, u32int index);
static fs_node_t *finddir(fs_node_t *node, const char *name);

void init_rootfs(void) {
	strcpy(rootfs_root.name, "");
	rootfs_root.mask = VFS_U_READ | VFS_U_WRITE | VFS_U_EXEC | VFS_G_READ |
					VFS_G_EXEC | VFS_O_READ | VFS_O_EXEC;
	rootfs_root.gid = rootfs_root.uid = 0;
	rootfs_root.flags = VFS_DIR;
	rootfs_root.inode = 0;
	rootfs_root.len = 0;
	rootfs_root.impl = 0;
	rootfs_root.ops.readdir = readdir;
	rootfs_root.ops.finddir = finddir;
	rootfs_root.fs_sb = &rootfs_sb;

	u32int i;
	for (i = 0; i < sizeof(names)/sizeof(char *); i++) {
		strcpy(subdirs[i].name, names[i]);
		subdirs[i].mask = VFS_U_READ | VFS_U_WRITE | VFS_U_EXEC | VFS_G_READ |
						VFS_G_EXEC | VFS_O_READ | VFS_O_EXEC;
		subdirs[i].gid = subdirs[i].uid = 0;
		subdirs[i].flags = VFS_DIR;
		subdirs[i].inode = i + 1;
		subdirs[i].len = 0;
		subdirs[i].impl = 0;
		subdirs[i].fs_sb = &rootfs_sb;
	}

	register_fs(&rootfs);
}

static struct superblock *return_sb(u32int flags, fs_node_t *dev) {
	flags = flags;
	dev = dev;
	return &rootfs_sb;
}

static struct dirent *readdir(fs_node_t *node, u32int index) {
	node = node;
	if (index >= sizeof(names)/sizeof(char *))
		return NULL;
	static struct dirent ret;
	ret.d_ino = index;
	strcpy(ret.d_name, names[index]);
	return &ret;
}

static fs_node_t *finddir(fs_node_t *node, const char *name) {
	node = node;
	u32int i;
	for (i = 0; i < sizeof(names)/sizeof(char *); i++) {
		if (strcmp(name, names[i]) == 0) {
			fs_node_t *ret = (fs_node_t *)kmalloc(sizeof(fs_node_t));
			memcpy(ret, &subdirs[i], sizeof(fs_node_t));
			return ret;
		}
	}
	return NULL;
}
