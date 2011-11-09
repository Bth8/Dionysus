/* rootfs.c - Quick-n-dirty fs for setting up sone basic mountpoints */
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

#include <fs/rootfs.h>
#include <common.h>
#include <vfs.h>
#include <string.h>

fs_node_t root;
char *names[] = {"bin", "dev"};
fs_node_t subdirs[sizeof(names)/sizeof(char *)];

static struct superblock *return_sb(struct file_system_type *fs, int, fs_node_t *dev);
struct file_system_type rootfs = {
	"rootfs",
	0,
	return_sb,
	NULL
};

static struct dirent *readdir(fs_node_t *node, u32int index);
static fs_node_t *finddir(fs_node_t *node, const char *name);

void init_rootfs(void) {
	strcpy(root.name, "");
	root.mask = VFS_U_READ | VFS_U_WRITE | VFS_U_EXEC | VFS_G_READ |
					VFS_G_EXEC | VFS_O_READ | VFS_O_EXEC;
	root.gid = root.uid = 0;
	root.flags = VFS_DIR;
	root.inode = 0;
	root.len = 0;
	root.impl = 0;
	root.ops.readdir = readdir;
	root.ops.finddir = finddir;

	u32int i;
	for (i = 0; i < sizeof(names)/sizeof(char *); i++) {
		strcpy(subdirs[i].name, names[i]);
		subdirs[i].mask = VFS_U_READ | VFS_U_WRITE | VFS_U_EXEC | VFS_G_READ |
						VFS_G_EXEC | VFS_O_READ | VFS_O_EXEC;
		subdirs[i].gid = subdirs[i].uid = 0;
		subdirs[i].flags = VFS_DIR;
		subdirs[i].inode = i;
		subdirs[i].len = 0;
		subdirs[i].impl = 0;
	}

	register_fs(&rootfs);
}

static struct superblock *return_sb(struct file_system_type *fs, int flags, fs_node_t *dev) {
	fs = fs;
	flags = flags;
	dev = dev;
	static struct superblock sb;
	sb.dev = 0;
	sb.blocksize = 0;
	sb.fs = &rootfs;
	sb.root = &root;
	return &sb;
}

static struct dirent *readdir(fs_node_t *node, u32int index) {
	ASSERT(node == &root);
	if (index >= sizeof(names)/sizeof(char *))
		return NULL;
	static struct dirent ret;
	ret.d_ino = index;
	strcpy(ret.d_name, names[index]);
	return &ret;
}

static fs_node_t *finddir(fs_node_t *node, const char *name) {
	ASSERT(node == &root);
	u32int i;
	for (i = 0; i < sizeof(names)/sizeof(char *); i++)
		if (strcmp(name, names[i]) == 0)
			return &subdirs[i];
	return NULL;
}
