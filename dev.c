/* dev.c - devfs function */
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

#include <common.h>
#include <vfs.h>
#include <dev.h>
#include <string.h>
#include <kmalloc.h>

fs_node_t dev_root;
struct dev_file *files = NULL;
struct chrdev_driver drivers[256];		// 1 for every valid major number

struct file_ops ops_default = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static struct superblock *return_sb(struct file_system_type *fs, int, fs_node_t *dev);
struct file_system_type dev_fs = {
	"dev",
	0,
	return_sb,
	NULL
};

struct superblock dev_sb = {
	0,
	0,
	&dev_fs,
	&dev_root
};

static struct dirent *readdir(fs_node_t *node, u32int index);
static struct fs_node *finddir(fs_node_t *node, const char *name);

void init_devfs(void) {
	strcpy(dev_root.name, "dev");
	dev_root.mask = VFS_U_READ | VFS_U_WRITE | VFS_U_EXEC | VFS_G_READ |
					VFS_G_EXEC | VFS_O_READ | VFS_O_EXEC;
	dev_root.gid = dev_root.uid = 0;	// Belong to root
	dev_root.flags = VFS_DIR;
	dev_root.inode = 0;
	dev_root.len = 0;
	dev_root.impl = 0;
	dev_root.ops.readdir = readdir;
	dev_root.ops.finddir = finddir;
	dev_root.fs_sb = &dev_sb;

	u32int i;
	for (i = 0; i < 256; i++) {
		drivers[i].name = "Default";
		drivers[i].ops = ops_default;
	}

	register_fs(&dev_fs);
}

static struct superblock *return_sb(struct file_system_type *fs, int flags, fs_node_t *dev) {
	fs = fs;		// Compiler complains otherwise
	flags = flags;
	dev = dev;
	return &dev_sb;
}

static struct dirent *readdir(fs_node_t *node, u32int index) {
	node = node;
	static struct dirent ret;
	u32int i;
	struct dev_file *filep = files;
	for (i = 0; i < index; filep = filep->next) {
		if (filep == NULL)
			return NULL;
		else
			++i;
	}
	ret.d_ino = index;
	strcpy(ret.d_name, filep->node.name);
	return &ret;
}

static struct fs_node *finddir(fs_node_t *node, const char *name) {
	node = node;
	struct dev_file *filep = files;
	while (strcmp(filep->node.name, name) != 0) {
		filep = filep->next;
		if (filep == NULL)
			return NULL;
	}
	fs_node_t *ret = (fs_node_t *)kmalloc(sizeof(fs_node_t));
	memcpy(ret, &filep->node, sizeof(fs_node_t));
	return ret;
}

s32int devfs_register(const char *name, u32int flags, u32int major,
					u32int minor, u32int mode, u32int uid, u32int gid) {
	if (strlen(name) >= NAME_MAX)
		return -1;
	if (major < 1)
		return -1;
	u32int i = 0;
	struct dev_file *filei = files, *newfile;
	if (files) {
		for (; filei->next != NULL; filei = filei->next) {			// Find last entry in files
			if (strcmp(filei->node.name, name) == 0)				// Make sure there aren't any name collisions
				return -1;
			i++;
		}
	}

	newfile = (struct dev_file *)kmalloc(sizeof(struct dev_file));	// Create a new file
	strcpy(newfile->node.name, name);
	newfile->node.mask = mode;
	newfile->node.gid = gid;
	newfile->node.uid = uid;
	newfile->node.flags = flags;
	newfile->node.inode = i;
	newfile->node.len = 0;
	newfile->node.impl = MKDEV(major, minor);
	newfile->node.ops = drivers[major - 1].ops;
	newfile->node.fs_sb = &dev_sb;

	newfile->next = NULL;

	if (files)
		filei->next = newfile;										// Place ourselves in the file list
	else
		files = newfile;

	return 0;
}

s32int register_chrdev(u32int major, const char *name, struct file_ops fops) {
	if (major == 0)															// Find an open major number if given zero
		for (major = 1; strcmp(drivers[major - 1].name, "Default") != 0; major++)
			if (major == 256)												// Gone too far, all drivers taken
				return -1;

	if (strcmp(drivers[major - 1].name, "Default") != 0)
		return -1;															// Already present

	drivers[major - 1].name = name;
	drivers[major - 1].ops = fops;

	if (files) {
		struct dev_file *filei;
		for (filei = files; filei->next != NULL; filei = filei->next)		// For files that existed before their majors were registered
			if (MAJOR(filei->node.impl) == major && filei->node.flags & VFS_CHARDEV)
				filei->node.ops = fops;
	}

	return major;
}
