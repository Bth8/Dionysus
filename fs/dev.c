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
#include <fs/dev.h>
#include <char.h>
#include <block.h>
#include <string.h>
#include <kmalloc.h>
#include <printf.h>
#include <structures/list.h>
#include <errno.h>

static struct superblock *return_sb(uint32_t, fs_node_t *dev);
static ssize_t read(fs_node_t *node, void *buf, size_t count, off_t off);
static ssize_t write(fs_node_t *node, const void *buf, size_t count, off_t off);
static int32_t open(fs_node_t *node, uint32_t flags);
static int32_t close(fs_node_t *node);
static int32_t readdir(fs_node_t *node, struct dirent *dirp, uint32_t index);
static fs_node_t *finddir(fs_node_t *node, const char *fname);
static int32_t chmod(fs_node_t *node, uint32_t mode);
static int32_t chown(fs_node_t *node, int32_t uid, int32_t gid);
static int32_t ioctl(fs_node_t *node, uint32_t req, void *data);
static int32_t create(fs_node_t *parent, const char *fname, uint32_t uid,
		uint32_t gid, uint32_t mode, dev_t dev);
static int32_t link(fs_node_t *parent, fs_node_t *child, const char *fname);
static int32_t unlink(fs_node_t *parent, const char *fname);

file_system_t dev_fs = {
	.flags = FS_NODEV,
	.get_super = return_sb,
};

void init_devfs(void) {
	register_fs("devfs", &dev_fs);
}

static struct superblock *return_sb(uint32_t flags, fs_node_t *dev) {
	if (dev)
		return NULL;

	struct superblock *sb = (struct superblock *)kmalloc(sizeof(struct superblock));
	if (!sb)
		return NULL;

	struct dev_file *root = (struct dev_file *)kmalloc(sizeof(struct dev_file));
	if (!root) {
		kfree(sb);
		return NULL;
	}

	root->node.inode = 0;
	root->node.gid = root->node.uid = 0;	// Belong to root
	root->node.mode = VFS_U_READ | VFS_U_WRITE | VFS_U_EXEC |
		VFS_G_READ | VFS_G_EXEC |
		VFS_O_READ | VFS_O_EXEC;
	root->node.flags = 0;
	root->node.len = 0;
	root->node.dev = 0;
	root->node.atime = 0;
	root->node.ctime = time(NULL);
	root->node.mtime = 0;
	root->node.ops.read = read;
	root->node.ops.write = write;
	root->node.ops.open = open;
	root->node.ops.close = close;
	root->node.ops.readdir = readdir;
	root->node.ops.finddir = finddir;
	root->node.ops.chmod = chmod;
	root->node.ops.chown = chown;
	root->node.ops.ioctl= ioctl;
	root->node.ops.create = create;
	root->node.ops.link = link;
	root->node.ops.unlink = unlink;
	root->node.fs_sb = sb;
	root->node.private_data = list_create();
	root->node.refcount = 0;
	root->node.nlink = 1;
	root->next = NULL;

	if (!root->node.private_data) {
		kfree(sb);
		kfree(root);
		return NULL;
	}

	sb->dev = NULL;
	sb->private_data = root;
	sb->root = &(root->node);
	sb->blocksize = KERNEL_BLOCKSIZE;
	sb->flags = flags;

	return sb;
}

static fs_node_t *get_inode(struct superblock *sb, uint32_t inode) {
	struct dev_file *file = (struct dev_file *)sb->private_data;
	while (file) {
		if (file->node.inode == inode)
			return &(file->node);
		if (file->node.inode > inode)
			break;
		file = file->next;
	}

	return NULL;
}

static void file_insert(struct superblock *sb, struct dev_file *file) {
	struct dev_file *iter = (struct dev_file *)sb->private_data;
	struct dev_file *prev = NULL;
	while (iter) {
		if (iter->node.inode > file->node.inode)
			break;
		prev = iter;
		iter = iter->next;
	}

	if (!prev) {
		file->next = (struct dev_file *)sb->private_data;
		sb->private_data = file;
	} else {
		file->next = prev->next;
		prev->next = file;
	}
}

static void file_remove(struct superblock *sb, uint32_t inode) {
	struct dev_file *iter = (struct dev_file *)sb->private_data;
	struct dev_file *prev = NULL;
	while (iter) {
		if (iter->node.inode == inode)
			break;
		if (iter->node.inode > inode)
			return;
		prev = iter;
		iter = iter->next;
	}

	if (!prev)
		sb->private_data = iter->next;
	else
		prev->next = iter->next;

	if (iter->node.mode & VFS_DIR)
		list_destroy(iter->node.private_data);
	kfree(iter);
}

static ssize_t read(fs_node_t *node, void *buf, size_t count, off_t off) {
	if (node->mode & VFS_CHARDEV) {
		struct chrdev_driver *driver = get_chrdev_driver(MAJOR(node->dev));
		if (driver && driver->ops.read)
			return driver->ops.read(node, buf, count, off);
	}

	return -EINVAL;
}

static ssize_t write(fs_node_t *node, const void *buf, size_t count, off_t off) {
	if (node->mode & VFS_CHARDEV) {
		struct chrdev_driver *driver = get_chrdev_driver(MAJOR(node->dev));
		if (driver && driver->ops.write)
			return driver->ops.write(node, buf, count, off);
	}

	return -EINVAL;
}


spinlock_t ref_lock = 0;

static int32_t open(fs_node_t *node, uint32_t flags) {
	spin_lock(&ref_lock);
	fs_node_t *master = get_inode(node->fs_sb, node->inode);
	if (!master)
		return -ENOENT;
	if (master->nlink == 0)
		return -ENOENT;

	int32_t ret = 0;

	if (node->mode & VFS_CHARDEV) {
		struct chrdev_driver *driver = get_chrdev_driver(MAJOR(node->dev));
		if (driver && driver->ops.open)
			ret = driver->ops.open(node, flags);
	} else if (node->mode & VFS_BLOCKDEV) {
		struct blkdev_driver *driver = get_blkdev_driver(MAJOR(node->dev));
		if (driver && driver->ops.open)
			ret = driver->ops.open(node, flags);
	}

	if (ret == 0)
		master->refcount++;
	spin_unlock(&ref_lock);

	return ret;
}

static int32_t close(fs_node_t *node) {
	spin_lock(&ref_lock);
	fs_node_t *master = get_inode(node->fs_sb, node->inode);
	if (!master)
		return 0;

	master->refcount--;

	int32_t ret = 0;

	if (master->refcount == 0) {
		if (node->mode & VFS_CHARDEV) {
			struct chrdev_driver *driver = get_chrdev_driver(MAJOR(node->dev));
			if (driver && driver->ops.close)
				ret = driver->ops.close(node);
		} else if (node->mode & VFS_BLOCKDEV) {
			struct blkdev_driver *driver = get_blkdev_driver(MAJOR(node->dev));
			if (driver && driver->ops.close)
				ret = driver->ops.close(node);
		}
		if (ret == 0 && master->nlink == 0)
			file_remove(node->fs_sb, node->inode);
	}
	spin_unlock(&ref_lock);

	return ret;
}

static int32_t readdir(fs_node_t *node, struct dirent *dirp, uint32_t index) {
	if (index == 0) {
		dirp->d_ino = 0;
		strcpy(dirp->d_name, ".");
		return 1;
	} else if (index == 1) {
		dirp->d_ino = 0;
		strcpy(dirp->d_name, "..");
		return 1;
	}

	index -= 2;
	node_t *entry = list_get_index((list_t *)node->private_data, index);
	if (!entry)
		return 0;

	memcpy(dirp, entry->data, sizeof(struct dirent));
	return 1;
}

static fs_node_t *finddir(fs_node_t *node, const char *fname) {
	node_t *entry;
	foreach(entry, ((list_t *)node->private_data)) {
		if (strcmp(((struct dirent *)entry->data)->d_name, fname) == 0)
			break;
	}

	if (!entry)
		return NULL;

	fs_node_t *master = get_inode(node->fs_sb, 
		((struct dirent *)entry->data)->d_ino);
	if (!master)
		return NULL;

	fs_node_t *ret = (fs_node_t *)kmalloc(sizeof(fs_node_t));
	if (!ret)
		return NULL;

	memcpy(ret, master, sizeof(fs_node_t));

	return ret;
}

static int32_t chmod(fs_node_t *node, uint32_t mode) {
	fs_node_t *master = get_inode(node->fs_sb, node->inode);
	if (!master)
		return -ENOENT;
	master->mode = (master->mode & VFS_TYPE_MASK) | (mode & VFS_PERM_MASK);
	node->mode = master->mode;

	return 0;
}

static int32_t chown(fs_node_t *node, int32_t uid, int32_t gid) {
	fs_node_t *master = get_inode(node->fs_sb, node->inode);
	if (!master)
		return -ENOENT;
	master->uid = node->uid = uid;
	master->gid = node->gid = gid;

	return 0;
}

static int32_t ioctl(fs_node_t *node, uint32_t req, void *data) {
	if (node->mode & VFS_CHARDEV) {
		struct chrdev_driver *driver = get_chrdev_driver(MAJOR(node->dev));
		if (driver && driver->ops.ioctl)
			driver->ops.ioctl(node, req, data);
	} else if (node->mode & VFS_BLOCKDEV) {
		struct blkdev_driver *driver = get_blkdev_driver(MAJOR(node->dev));
		if (driver && driver->ops.ioctl)
			return driver->ops.ioctl(node, req, data);
	}

	return -ENOTTY;
}

static uint32_t get_empty_inode(struct superblock *sb) {
	struct dev_file *file = (struct dev_file *)sb->private_data;
	uint32_t inode = 0;
	while (file) {
		if (inode < file->node.inode)
			break;
		inode = file->node.inode + 1;
		file = file->next;
	}

	return inode;
}

static int32_t create(fs_node_t *parent, const char *fname, uint32_t uid,
		uint32_t gid, uint32_t mode, dev_t dev) {
	if ((mode & VFS_TYPE_MASK) != VFS_DIR &&
			(mode & VFS_TYPE_MASK) != VFS_BLOCKDEV &&
			(mode & VFS_TYPE_MASK) != VFS_CHARDEV)
		return -EINVAL;
	
	struct dev_file *new_file = (struct dev_file *)kmalloc(sizeof(struct dev_file));
	if (!new_file)
		return -ENOMEM;
	struct dirent *dirp = (struct dirent *)kmalloc(sizeof(struct dirent));
	if (!dirp) {
		kfree(new_file);
		return -ENOMEM;
	}

	new_file->node.inode = get_empty_inode(parent->fs_sb);
	new_file->node.uid = uid;
	new_file->node.gid = gid;
	new_file->node.mode = mode;
	new_file->node.flags = 0;
	new_file->node.len = 0;
	new_file->node.dev = dev;
	new_file->node.atime = 0;
	new_file->node.mtime = 0;
	new_file->node.ctime = time(NULL);
	new_file->node.ops.read = read;
	new_file->node.ops.write = write;
	new_file->node.ops.open = open;
	new_file->node.ops.close = close;
	new_file->node.ops.readdir = readdir;
	new_file->node.ops.finddir = finddir;
	new_file->node.ops.chmod = chmod;
	new_file->node.ops.chown = chown;
	new_file->node.ops.ioctl = ioctl;
	new_file->node.ops.create = create;
	new_file->node.ops.link = link;
	new_file->node.ops.unlink = unlink;
	new_file->node.fs_sb = parent->fs_sb;
	if (mode & VFS_DIR) {
		new_file->node.private_data = list_create();
		if (!new_file->node.private_data) {
			kfree(dirp);
			kfree(new_file);
			return -ENOMEM;
		}
	} else {
		new_file->node.private_data = NULL;
	}
	new_file->node.refcount = 0;
	new_file->node.nlink = 1;

	file_insert(parent->fs_sb, new_file);

	dirp->d_ino = new_file->node.inode;
	strcpy(dirp->d_name, fname);
	list_insert((list_t *)parent->private_data, dirp);

	return 0;
}

static int32_t link(fs_node_t *parent, fs_node_t *child, const char *fname) {
	fs_node_t *master = get_inode(child->fs_sb, child->inode);
	if (!child)
		return -ENOTDIR;

	struct dirent *dirp = (struct dirent *)kmalloc(sizeof(struct dirent));
	if (!dirp)
		return -ENOMEM;
	dirp->d_ino = child->inode;
	strcpy(dirp->d_name, fname);

	child->nlink = ++(master->nlink);
	list_insert((list_t *)parent->private_data, dirp);

	return 0;
}

static int32_t unlink(fs_node_t *parent, const char *fname) {
	fs_node_t *master = get_inode(parent->fs_sb, parent->inode);
	if (!master)
		return -ENOTDIR;

	node_t *iter;
	foreach(iter, ((list_t *)parent->private_data)) {
		if (strcmp(((struct dirent *)iter->data)->d_name, fname) == 0)
			break;
	}

	if (!iter)
		return -ENOENT;

	fs_node_t *child = get_inode(parent->fs_sb, 
		((struct dirent *)iter->data)->d_ino);
	if (!child) // This... would not be expected
		return -ENOENT;

	list_remove((list_t *)parent->private_data, iter);

	if (--child->nlink == 0 && child->refcount == 0)
		file_remove(parent->fs_sb, child->inode);

	return 0;
}

/*
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
*/