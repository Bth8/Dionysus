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
#include <ordered_array.h>
#include <monitor.h>

fs_node_t dev_root;
struct dev_file *files = NULL;
struct chrdev_driver char_drivers[256];		// 1 for every valid major number
struct blkdev_driver blk_drivers[256];

static struct superblock *return_sb(u32int, fs_node_t *dev);
struct file_system_type dev_fs = {
	.name = "dev",
	.flags = FS_NODEV,
	.get_super = return_sb,
};

struct superblock dev_sb = {
	.root = &dev_root,
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
		char_drivers[i].name = "Default";
		char_drivers[i].ops = (struct file_ops){NULL, NULL, NULL, NULL, NULL, NULL, NULL};
		blk_drivers[i].name = "Default";
		blk_drivers[i].read = NULL;
		blk_drivers[i].write = NULL;
		blk_drivers[i].ioctl = NULL;
	}

	register_fs(&dev_fs);
}

static struct superblock *return_sb(u32int flags, fs_node_t *dev) {
	flags = flags;	// Compiler complains otherwise
	dev = dev;
	return &dev_sb;
}

s32int read_blkdev(u32int major, u32int minor, u32int count, u32int off, char *buf);
s32int write_blkdev(u32int major, u32int minor, u32int count, u32int off, const char *buf);

static u32int read(fs_node_t *node, void *buf, u32int count, u32int off) {
	if (node->flags & VFS_CHARDEV) {
		if (char_drivers[MAJOR(node->impl) - 1].ops.read)
			return char_drivers[MAJOR(node->impl) - 1].ops.read(node, buf, count, off);
	} else if (node->flags & VFS_BLOCKDEV) {
		if (read_blkdev(MAJOR(node->impl), MINOR(node->impl), count, off, buf) == 0)
			return count;
	}
			
	return 0;
}

static u32int write(fs_node_t *node, const void *buf, u32int count, u32int off) {
	if (node->flags & VFS_CHARDEV) {
		if (char_drivers[MAJOR(node->impl) - 1].ops.write)
			return char_drivers[MAJOR(node->impl) - 1].ops.write(node, buf, count, off);
	} else if (node->flags & VFS_BLOCKDEV) {
		if (write_blkdev(MAJOR(node->impl), MINOR(node->impl), count, off, buf) == 0)
			return count;
	}

	return 0;
}

static void open(fs_node_t *node, u32int flags) {
	if (node->flags & VFS_CHARDEV) {
		if (char_drivers[MAJOR(node->impl) - 1].ops.open)
			return char_drivers[MAJOR(node->impl) - 1].ops.open(node, flags);
	} else if (node->flags & VFS_BLOCKDEV) {
		struct blockdev *dev = NULL;
		u32int i, major = MAJOR(node->impl), minor = MINOR(node->impl);
		for (i = 0; i < blk_drivers[major - 1].devs.size; i++)
			if ((dev = lookup_ordered_array(i, &blk_drivers[major - 1].devs))->minor == minor)
				break;
		if (dev == NULL || dev->minor != minor)
			return;

		if (dev->driver->write)
			node->mask = (flags & O_RDWR);
		else
			node->mask = (flags & O_RDONLY);

		node->len = dev->capacity * dev->block_size;
	}
		
}

static void close(fs_node_t *node) {
	if (node->flags & VFS_CHARDEV)
		if (char_drivers[MAJOR(node->impl) - 1].ops.close)
			return char_drivers[MAJOR(node->impl) - 1].ops.close(node);
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

static s32int ioctl(fs_node_t *node, u32int request, void *ptr) {
	if (node->flags & VFS_CHARDEV) {
		if (char_drivers[MAJOR(node->impl) - 1].ops.ioctl)
			return char_drivers[MAJOR(node->impl) - 1].ops.ioctl(node, request, ptr);
	} else if (node->flags & VFS_BLOCKDEV) {
		if (blk_drivers[MAJOR(node->impl) - 1].ioctl)
			return blk_drivers[MAJOR(node->impl) - 1].ioctl(MINOR(node->impl) / 16, request, ptr);
	}

	return -1;
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
	newfile->node.ops.read = read;
	newfile->node.ops.write = write;
	newfile->node.ops.open = open;
	newfile->node.ops.close = close;
	newfile->node.ops.ioctl = ioctl;
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
		for (major = 1; strcmp(char_drivers[major - 1].name, "Default") != 0; major++)
			if (major == 256)												// Gone too far, all drivers taken
				return -1;
	else if (strcmp(char_drivers[major - 1].name, "Default") != 0)
		return -1;															// Already present

	char_drivers[major - 1].name = name;
	char_drivers[major - 1].ops = fops;

	monitor_write("Chardev driver ");
	monitor_write(name);
	monitor_write(" added\n");

	return major;
}

s32int detect_partitions(struct blockdev *dev) {
	struct mbr mbr;
	u32int nblocks = sizeof(mbr) / dev->block_size;
	if (dev->driver->read(dev->minor / 16, 0, nblocks, &mbr))	// Get the disk's MBR
		return -1;

	if (mbr.magic[0] != 0x55 || mbr.magic[1] != 0xAA) {			// Valid?
		monitor_write("Warning: invalid partition table driver ");
		monitor_write(dev->driver->name);
		monitor_write(" device ");
		monitor_write_udec(dev->minor/16);
		monitor_put('\n');
		return 0;
	}

	u32int i;
	for (i = 0; i < 4; i++) {
		if (mbr.partitions[i].sys_id != 0) {	// We've got a partition!
			struct blockdev *part = (struct blockdev *)kmalloc(sizeof(struct blockdev));
			part->driver = dev->driver;
			part->minor = dev->minor + i + 1;
			part->block_size = dev->block_size;
			part->offset = mbr.partitions[i].rel_sect;
			part->capacity = mbr.partitions[i].nsects;
			insert_ordered_array(part, &dev->driver->devs);
		}
	}

	return 0;
}

static u32int lessthan_blkdev(type_t a, type_t b) {
	return (((struct blockdev *)a)->minor < ((struct blockdev *)b)->minor) ? 1 : 0;
}

s32int register_blkdev(u32int major, const char *name,
						u32int (*read)(u32int, u32int, u32int, void*),
						u32int (*write)(u32int, u32int, u32int, const void*),
						s32int (*ioctl)(u32int, u32int, void *),
						u32int nreal, u32int sectsize, u32int *drv_sizes) {

	if (!read)																// We have to have a read
		return -1;															// Not necessarily write or ioctl, but read

	if (major == 0)															// Find an open major
		for (major = 1; strcmp(blk_drivers[major - 1].name, "Default") != 0; major++)
			if (major == 256)												// All major numbers allocated
				return -1;
	else if (strcmp(blk_drivers[major - 1].name, "Default") != 0)			// Already present
		return -1;

	blk_drivers[major - 1].name = name;										// Set up fields
	blk_drivers[major - 1].read = read;
	blk_drivers[major - 1].write = write;
	blk_drivers[major - 1].ioctl = ioctl;
	blk_drivers[major - 1].nreal = nreal;
	blk_drivers[major - 1].devs = create_ordered_array(nreal * 16, lessthan_blkdev);

	monitor_write("Blockdev driver ");
	monitor_write(name);
	monitor_write(" added\n");

	u32int i;
	for (i = 0; i < nreal; i++) {											// Add 
		struct blockdev *dev = (struct blockdev *)kmalloc(sizeof(struct blockdev));
		dev->driver = &blk_drivers[major - 1];
		dev->minor = i*16;
		dev->block_size = sectsize;
		dev->offset = 0;
		dev->capacity = drv_sizes[i];
		insert_ordered_array(dev, &blk_drivers[major - 1].devs);
		ASSERT(detect_partitions(dev) != -1);
	}

	return major;
}

s32int read_blkdev(u32int major, u32int minor, u32int count, u32int off, char *buf) {
	struct blockdev *dev = NULL;
	u32int i;
	for (i = 0; i < blk_drivers[major - 1].devs.size; i++)
		if ((dev = lookup_ordered_array(i, &blk_drivers[major - 1].devs))->minor == minor)
			break;
	if (dev == NULL || dev->minor != minor)
		return -1;

	u32int block = off / dev->block_size + dev->offset;
	u32int delta = off % dev->block_size;
	char *dst = buf;

	// Only want part of first block
	if (delta) {
		char *tmp = (char *)kmalloc(dev->block_size);
		if (dev->driver->read(dev->minor / 16, block, 1, tmp)) {
			kfree(tmp);
			return -1;
		}
		memcpy(dst, tmp + delta, dev->block_size - delta);
		kfree(tmp);
		dst += dev->block_size - delta;
		block++;
	}

	u32int nblocks = (off + count) / dev->block_size - (block - dev->offset);
	delta = (off + count) % dev->block_size;

	if (dev->driver->read(dev->minor / 16, block, nblocks, dst))
		return -1;

	dst += nblocks * dev->block_size;
	block += nblocks;

	// Only want part of last block
	if (delta) {
		char *tmp = (char *)kmalloc(dev->block_size);
		if (dev->driver->read(dev->minor / 16, block, 1, tmp)) {
			kfree(tmp);
			return -1;
		}
		memcpy(dst, tmp, delta);
		kfree(tmp);
	}

	return 0;
}

s32int write_blkdev(u32int major, u32int minor, u32int count, u32int off, const char *buf) {
	struct blockdev *dev = NULL;
	u32int i;
	for (i = 0; i < blk_drivers[major - 1].devs.size; i++)
		if ((dev = lookup_ordered_array(i, &blk_drivers[major - 1].devs))->minor == minor)
			break;
	if (dev == NULL || dev->minor != minor)
		return -1;

	if (!dev->driver->write) // Sanity check
		return -1;

	u32int block = off / dev->block_size + dev->offset;
	u32int delta = off % dev->block_size;
	const char *src = buf;

	if (delta) {
		char *tmp = (char *)kmalloc(dev->block_size);
		if (dev->driver->read(dev->minor / 16, block, 1, tmp)) {
			kfree(tmp);
			return -1;
		}
		memcpy(tmp + delta, src, dev->block_size - delta);
		if (dev->driver->write(dev->minor / 16, block, 1, tmp)) {
			kfree(tmp);
			return -1;
		}
		kfree(tmp);
		src += dev->block_size - delta;
		block++;
	}

	u32int nblocks = (off + count) / dev->block_size - (block - dev->offset);
	delta = (off + count) % dev->block_size;

	if (dev->driver->write(dev->minor / 16, block, nblocks, src))
		return -1;

	src += nblocks * dev->block_size;
	block += nblocks;

	if (delta) {
		char *tmp = (char *)kmalloc(dev->block_size);
		if (dev->driver->read(dev->minor / 16, block, 1, tmp)) {
			kfree(tmp);
			return -1;
		}
		memcpy(tmp, src, delta);
		if (dev->driver->write(dev->minor / 16, block, 1, tmp)) {
			kfree(tmp);
			return -1;
		}
		kfree(tmp);
	}

	return 0;
}
