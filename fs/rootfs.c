/* rootfs.c - Quick-n-dirty fs for setting up sone basic mountpoints */

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

#include <fs/rootfs.h>
#include <common.h>
#include <vfs.h>
#include <string.h>
#include <kmalloc.h>

fs_node_t rootfs_root;
char *names[] = {"real_root", "dev"};
fs_node_t subdirs[sizeof(names)/sizeof(char *)];

static struct superblock *return_sb(uint32_t flags, fs_node_t *dev);
struct file_system_type rootfs = {
	.name = "rootfs",
	.flags = FS_NODEV,
	.get_super = return_sb,
};

struct superblock rootfs_sb = {
	.root = &rootfs_root
};

static int readdir(fs_node_t *node, struct dirent *dirp, uint32_t index);
static fs_node_t *finddir(fs_node_t *node, const char *name);
// Do literally nothing
static int open(fs_node_t *file, uint32_t flags) {file = file; flags = flags; return 0;}
static int close(fs_node_t *file) { file = file; return 0;}

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
	rootfs_root.ops.open = open;
	rootfs_root.ops.close = close;
	rootfs_root.fs_sb = &rootfs_sb;

	uint32_t i;
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

static struct superblock *return_sb(uint32_t flags, fs_node_t *dev) {
	flags = flags;
	dev = dev;
	return &rootfs_sb;
}

static int readdir(fs_node_t *node, struct dirent *dirp, uint32_t index) {
	node = node;
	if (index >= sizeof(names)/sizeof(char *))
		return -1;
	dirp->d_ino = index;
	strcpy(dirp->d_name, names[index]);
	return 0;
}

static fs_node_t *finddir(fs_node_t *node, const char *name) {
	node = node;
	uint32_t i;
	for (i = 0; i < sizeof(names)/sizeof(char *); i++) {
		if (strcmp(name, names[i]) == 0) {
			fs_node_t *ret = (fs_node_t *)kmalloc(sizeof(fs_node_t));
			memcpy(ret, &subdirs[i], sizeof(fs_node_t));
			return ret;
		}
	}
	return NULL;
}
