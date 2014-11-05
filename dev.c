/* dev.c - devfs function */

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
#include <vfs.h>
#include <dev.h>
#include <string.h>
#include <kmalloc.h>
#include <structures/ordered_array.h>
#include <printf.h>
#include <errno.h>

fs_node_t dev_root;
struct dev_file *files = NULL;
struct chrdev_driver char_drivers[256];		// 1 for every valid major number
struct blkdev_driver blk_drivers[256];

static struct superblock *return_sb(uint32_t, fs_node_t *dev);
struct file_system_type dev_fs = {
	.name = "dev",
	.flags = FS_NODEV,
	.get_super = return_sb,
};

struct superblock dev_sb = {
	.root = &dev_root,
};

static int readdir(fs_node_t *node, struct dirent *dirp, uint32_t index);
static struct fs_node *finddir(fs_node_t *node, const char *name);
static int root_open(fs_node_t *file, uint32_t flags) { return 0;}
static int root_close(fs_node_t *file) { return 0;}

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
	dev_root.ops.open = root_open;
	dev_root.ops.close = root_close;
	dev_root.fs_sb = &dev_sb;

	uint32_t i;
	for (i = 0; i < 256; i++) {
		char_drivers[i].name = "Default";
		char_drivers[i].ops = (struct file_ops){ NULL };
		blk_drivers[i].name = "Default";
		blk_drivers[i].read = NULL;
		blk_drivers[i].write = NULL;
		blk_drivers[i].ioctl = NULL;
	}

	register_fs(&dev_fs);
}

static struct superblock *return_sb(uint32_t flags, fs_node_t *dev) {
	return &dev_sb;
}

dev_t get_dev(fs_node_t *dev) {
	if (!dev)
		return 0;
	if (!(dev->flags & (VFS_BLOCKDEV | VFS_CHARDEV)))
		return 0;

	return dev->impl;
}

uint32_t read_blkdev(uint32_t major, uint32_t minor, size_t count, off_t off,
		char *buf);
uint32_t write_blkdev(uint32_t major, uint32_t minor, size_t count, off_t off,
		const char *buf);

static ssize_t read(fs_node_t *node, void *buf, size_t count, off_t off) {
	if (node->flags & VFS_CHARDEV) {
		if (char_drivers[MAJOR(node->impl) - 1].ops.read)
			return char_drivers[MAJOR(node->impl) - 1].ops.read(node,
					buf, count, off);
	} else if (node->flags & VFS_BLOCKDEV)
		return read_blkdev(MAJOR(node->impl), MINOR(node->impl), count,
				off, buf);

	return 0;
}

static ssize_t write(fs_node_t *node, const void *buf, size_t count,
		off_t off) {
	if (node->flags & VFS_CHARDEV) {
		if (char_drivers[MAJOR(node->impl) - 1].ops.write)
			return char_drivers[MAJOR(node->impl) - 1].ops.write(node, buf,
					count, off);
	} else if (node->flags & VFS_BLOCKDEV)
		return write_blkdev(MAJOR(node->impl), MINOR(node->impl), count,
				off, buf);

	return 0;
}

static int open(fs_node_t *node, uint32_t flags) {
	if (node->flags & VFS_CHARDEV) {
		if (char_drivers[MAJOR(node->impl) - 1].ops.open)
			return char_drivers[MAJOR(node->impl) - 1].ops.open(node, flags);
	} else if (node->flags & VFS_BLOCKDEV) {
		struct blockdev *dev = NULL;
		uint32_t i, major = MAJOR(node->impl), minor = MINOR(node->impl);
		for (i = 0; i < blk_drivers[major - 1].devs.size; i++) {
			dev = lookup_ordered_array(i, &blk_drivers[major - 1].devs);
			if (dev->minor == minor)
				break;
		}

		if (dev == NULL || dev->minor != minor)
			return -1;

		if (dev->driver->write)
			node->mask = (flags & O_RDWR);
		else
			node->mask = (flags & O_RDONLY);

		node->len = dev->capacity * dev->block_size;
		return 0;
	}

	// can't be reached
	return -1;
}

static int close(fs_node_t *node) {
	if (node->flags & VFS_CHARDEV) {
		if (char_drivers[MAJOR(node->impl) - 1].ops.close)
			return char_drivers[MAJOR(node->impl) - 1].ops.close(node);
	} else {
		return 0;
	}

	// Can't be reached
	return -1;
}

static int readdir(fs_node_t *node, struct dirent *dirp, uint32_t index) {
	node = node;
	uint32_t i;
	struct dev_file *filep = files;
	for (i = 0; i < index; filep = filep->next) {
		if (filep == NULL)
			return -1;
		else
			++i;
	}
	dirp->d_ino = index;
	strcpy(dirp->d_name, filep->node.name);
	return 0;
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
	if (!ret)
		return NULL;

	memcpy(ret, &filep->node, sizeof(fs_node_t));
	return ret;
}

static int32_t ioctl(fs_node_t *node, uint32_t request, void *ptr) {
	if (node->flags & VFS_CHARDEV) {
		if (char_drivers[MAJOR(node->impl) - 1].ops.ioctl)
			return char_drivers[MAJOR(node->impl) - 1].ops.ioctl(node,
					request, ptr);
	} else if (node->flags & VFS_BLOCKDEV) {
		if (blk_drivers[MAJOR(node->impl) - 1].ioctl)
			return blk_drivers[MAJOR(node->impl) - 1].ioctl(
					MINOR(node->impl) / 16, request, ptr);
	}

	return -ENOTTY;
}

static int stat(fs_node_t *node, struct stat *buff) {
	if (node->flags & VFS_CHARDEV) {
		if (char_drivers[MAJOR(node->impl) - 1].ops.stat)
			return char_drivers[MAJOR(node->impl) - 1].ops.stat(node, buff);
	} else if (node->flags & VFS_BLOCKDEV) {
		if (MINOR(node->impl))
			buff->st_dev = MKDEV(MAJOR(node->impl), 0);
		else
			buff->st_dev = node->impl;

		buff->st_ino = node->inode;
		buff->st_mode = node->mask;
		buff->st_nlink = 1;
		buff->st_uid = node->uid;
		buff->st_gid = node->gid;
		buff->st_rdev = node->impl;
		struct blockdev *dev = NULL;
		uint32_t i;
		for (i = 0; i < blk_drivers[MAJOR(node->impl) - 1].devs.size; i++) {
			dev = lookup_ordered_array(i,
					&blk_drivers[MAJOR(node->impl)-1].devs);
			if (dev->minor == MINOR(node->impl))
				break;
		}
		if (dev == NULL || dev->minor != MINOR(node->impl))
			return -ENOENT;
		buff->st_size = dev->capacity * KERNEL_BLOCKSIZE;
		buff->st_blksize = dev->block_size;
		buff->st_blocks = dev->capacity;
		return 0;
	}

	return -ENOENT;
}

int32_t devfs_register(const char *name, uint32_t flags, uint32_t major,
		uint32_t minor, uint32_t mode, uint32_t uid, uint32_t gid) {
	if (strlen(name) >= NAME_MAX)
		return -ENAMETOOLONG;
	if (major < 1)
		return -1;
	uint32_t i = 0;
	struct dev_file *filei = files, *newfile;
	if (files) {
		//Check for name collisions
		for (; filei->next != NULL; filei = filei->next) {
			if (strcmp(filei->node.name, name) == 0)
				return -EEXIST;
			i++;
		}
	}

	newfile = (struct dev_file *)kmalloc(sizeof(struct dev_file));
	if (!newfile)
		return -ENOMEM;

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
	newfile->node.ops.stat = stat;
	newfile->node.fs_sb = &dev_sb;

	newfile->next = NULL;

	// Place ourselves in the file list
	if (files)
		filei->next = newfile;
	else
		files = newfile;

	return 0;
}

int32_t register_chrdev(uint32_t major, const char *name, struct file_ops fops) {
	// Find an open major number if given zero
	if (major == 0)
		// Max 256
		for (major = 1; strcmp(char_drivers[major - 1].name, "Default") != 0;
				major++)
			if (major == 256)
				return -1;

	// Already present
	else if (strcmp(char_drivers[major - 1].name, "Default") != 0)
		return -1;

	char_drivers[major - 1].name = name;
	char_drivers[major - 1].ops = fops;

	printf("Chardev driver %s added\n", name);

	return major;
}

int32_t detect_partitions(struct blockdev *dev) {
	struct mbr mbr;
	uint32_t nblocks = sizeof(mbr) / dev->block_size;
	// Get the disk's MBR
	if (dev->driver->read(dev->minor / 16, 0, nblocks, &mbr))
		return -1;

	// Valid?
	if (mbr.magic[0] != 0x55 || mbr.magic[1] != 0xAA) {
		printf("Warning: invalid partition table\n\tDriver: %s Device: %u\n",
				dev->driver->name, dev->minor/16);
		return 0;
	}

	uint32_t i;
	for (i = 0; i < 4; i++) {
		if (mbr.partitions[i].sys_id != 0) {
			struct blockdev *part =
				(struct blockdev *)kmalloc(sizeof(struct blockdev));
			if (part == NULL)
				return -ENOMEM;
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

static uint32_t lessthan_blkdev(type_t a, type_t b) {
	return (((struct blockdev *)a)->minor < ((struct blockdev *)b)->minor) ?
		1 : 0;
}

int32_t register_blkdev(uint32_t major, const char *name,
		uint32_t (*read)(uint32_t, uint32_t, uint32_t, void*),
		uint32_t (*write)(uint32_t, uint32_t, uint32_t, const void*),
		int32_t (*ioctl)(uint32_t, uint32_t, void *),
		uint32_t nreal, uint32_t sectsize, uint32_t *drv_sizes) {

	// Having read is a must
	if (!read)
		return -1;

	// Find an open major
	if (major == 0)
		for (major = 1; strcmp(blk_drivers[major - 1].name, "Default") != 0;
				major++)
			if (major == 256)
				return -1;

	// Already present
	else if (strcmp(blk_drivers[major - 1].name, "Default") != 0)
		return -1;

	// Set up fields
	blk_drivers[major - 1].name = name;
	blk_drivers[major - 1].read = read;
	blk_drivers[major - 1].write = write;
	blk_drivers[major - 1].ioctl = ioctl;
	blk_drivers[major - 1].nreal = nreal;
	blk_drivers[major - 1].devs = create_ordered_array(nreal * 16,
			lessthan_blkdev);

	printf("Blockdev driver %s added\n", name);

	uint32_t i;
	for (i = 0; i < nreal; i++) {
		struct blockdev *dev =
			(struct blockdev *)kmalloc(sizeof(struct blockdev));
		if (!dev)
			return -ENOMEM;

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

uint32_t read_blkdev(uint32_t major, uint32_t minor, size_t count, off_t off,
		char *buf) {
	struct blockdev *dev = NULL;
	uint32_t i;
	for (i = 0; i < blk_drivers[major - 1].devs.size; i++) {
		dev = lookup_ordered_array(i, &blk_drivers[major - 1].devs);
		if (dev->minor == minor)
			break;
	}
	if (dev == NULL || dev->minor != minor)
		return 0;

	uint32_t block = off / dev->block_size + dev->offset;
	off %= dev->block_size;
	size_t read = 0;

	if (off + count >= dev->block_size) {
		char *tmp = (char *)kmalloc(dev->block_size);
		if (!tmp)
			return 0;
		if (dev->driver->read(dev->minor / 16, block, 1, tmp)) {
			kfree(tmp);
			return 0;
		}
		memcpy(buf, tmp + off, dev->block_size - off);
		count -= dev->block_size - off;
		read += dev->block_size - off;
		off = 0;
		block++;
		kfree(tmp);
	}

	uint32_t nblocks = count / dev->block_size;
	count %= dev->block_size;

	if (nblocks &&
			dev->driver->read(dev->minor / 16, block, nblocks, buf + read))
		return read;

	read += nblocks * dev->block_size;
	block += nblocks;

	if (count) {
		char *tmp = (char *)kmalloc(dev->block_size);
		if (!tmp)
			return read;
		if (dev->driver->read(dev->minor / 16, block, 1, tmp)) {
			kfree(tmp);
			return read;
		}
		memcpy(buf + read, tmp + off, count);
		read += count;
		kfree(tmp);
	}

	return read;
}

uint32_t write_blkdev(uint32_t major, uint32_t minor, size_t count, off_t off,
		const char *buf) {
	struct blockdev *dev = NULL;
	uint32_t i;
	for (i = 0; i < blk_drivers[major - 1].devs.size; i++) {
		dev = lookup_ordered_array(i, &blk_drivers[major - 1].devs);
		if (dev->minor == minor)
			break;
	}
	if (dev == NULL || dev->minor != minor)
		return 0;

	if (!dev->driver->write) // Sanity check
		return 0;

	uint32_t block = off / dev->block_size + dev->offset;
	off %= dev->block_size;
	size_t written = 0;

	if (off + count >= dev->block_size) {
		char *tmp = (char *)kmalloc(dev->block_size);
		if (!tmp)
			return 0;
		if (dev->driver->read(dev->minor / 16, block, 1, tmp)) {
			kfree(tmp);
			return 0;
		}
		memcpy(tmp + off, buf, dev->block_size - off);
		if (dev->driver->write(dev->minor / 16, block, 1, tmp)) {
			kfree(tmp);
			return 0;
		}
		count -= dev->block_size - off;
		written += dev->block_size - off;
		off = 0;
		block++;
		kfree(tmp);
	}

	uint32_t nblocks = count / dev->block_size;
	count %= dev->block_size;

	if (nblocks && dev->driver->write(dev->minor / 16, block, nblocks,
				buf + written))
		return written;

	written += nblocks * dev->block_size;
	block += nblocks;

	if (count) {
		char *tmp = (char*)kmalloc(dev->block_size);
		if (!tmp)
			return written;
		if (dev->driver->read(dev->minor / 16, block, 1, tmp)) {
			kfree(tmp);
			return written;
		}
		memcpy(tmp + off, buf + written, count);
		if (dev->driver->write(dev->minor / 16, block, 1, tmp)) {
			kfree(tmp);
			return written;
		}
		kfree(tmp);
		written += count;
	}

	return written;
}
