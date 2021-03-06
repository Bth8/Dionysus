/* dev.c - devfs function */

/* Copyright (C) 2015 Bth8 <bth8fwd@gmail.com>
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
#include <structures/mutex.h>
#include <errno.h>
#include <block.h>

static struct superblock *return_sb(dev_t dev, uint32_t flags);
static ssize_t read(fs_node_t *node, void *buf, size_t count, off_t off);
static ssize_t write(fs_node_t *node, const void *buf, size_t count, off_t off);
static int32_t open(fs_node_t *node, uint32_t flags);
static int32_t close(fs_node_t *node);
static int32_t readdir(fs_node_t *node, struct dirent *dirp, uint32_t index);
static fs_node_t *finddir(fs_node_t *node, const char *fname);
static int32_t chmod(fs_node_t *node, mode_t mode);
static int32_t chown(fs_node_t *node, uid_t uid, uid_t gid);
static int32_t ioctl(fs_node_t *node, uint32_t req, void *data);
static int32_t create(fs_node_t *parent, const char *fname, uid_t uid,
		uid_t gid, mode_t mode, dev_t dev);
static int32_t link(fs_node_t *parent, fs_node_t *child, const char *fname);
static int32_t unlink(fs_node_t *parent, const char *fname);

file_system_t dev_fs = {
	.flags = FS_NODEV,
	.get_super = return_sb,
};

struct file_ops devfs_ops = {
	.read = read,
	.write = write,
	.open = open,
	.close = close,
	.readdir = readdir,
	.finddir = finddir,
	.chmod = chmod,
	.chown = chown,
	.ioctl = ioctl,
	.create = create,
	.link = link,
	.unlink = unlink
};

void init_devfs(void) {
	register_fs("devfs", &dev_fs);
}

static int32_t close_fs(struct superblock *sb, uint32_t force) {
	acquire_semaphore_write((rw_sem_t *)sb->private_data);

	if (!force) {
		struct dev_file *iter = container_of(sb->root, struct dev_file, node);
		while (iter) {
			if (iter->node.refcount > 0)
				return -EBUSY;
			iter = iter->next;
		}
	}

	struct dev_file *iter = container_of(sb->root, struct dev_file, node);
	while (iter) {
		struct dev_file *next = iter->next;
		if (iter->node.mode & VFS_DIR)
			list_destroy((list_t *)iter->node.private_data);
		kfree(iter);
		iter = next;
	}

	destroy_rw_semaphore((rw_sem_t *)sb->private_data);
	kfree(sb);
	return 0;
}

static struct superblock *return_sb(dev_t dev, uint32_t flags) {
	if (dev)
		return NULL;

	struct superblock *sb = (struct superblock *)kmalloc(sizeof(struct superblock));
	if (!sb)
		return NULL;

	rw_sem_t *sem = create_rw_semaphore(32);
	if (!sem) {
		kfree(sb);
		return NULL;
	}

	struct dev_file *root = (struct dev_file *)kmalloc(sizeof(struct dev_file));
	if (!root) {
		destroy_rw_semaphore(sem);
		kfree(sb);
		return NULL;
	}

	root->node.inode = 0;
	root->node.gid = root->node.uid = 0;	// Belong to root
	root->node.mode = VFS_U_READ | VFS_U_WRITE | VFS_U_EXEC |
		VFS_G_READ | VFS_G_EXEC |
		VFS_O_READ | VFS_O_EXEC;
	root->node.mode |= VFS_DIR;
	root->node.flags = 0;
	root->node.len = 0;
	root->node.dev = 0;
	root->node.atime = 0;
	root->node.ctime = time(NULL);
	root->node.mtime = 0;
	memcpy(&(root->node.ops), &devfs_ops, sizeof(struct file_ops));
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

	sb->dev = dev;
	sb->private_data = sem;
	sb->root = &(root->node);
	sb->flags = flags;
	sb->close_fs = close_fs;

	return sb;
}

static fs_node_t *get_inode(struct superblock *sb, ino_t inode) {
	struct dev_file *file = container_of(sb->root, struct dev_file, node);
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
	struct dev_file *iter = container_of(sb->root, struct dev_file, node);
	struct dev_file *prev = NULL;
	while (iter) {
		if (iter->node.inode > file->node.inode)
			break;
		prev = iter;
		iter = iter->next;
	}

	// We're assuming that the inode is not 0 and is unique
	file->next = prev->next;
	prev->next = file;
}

static void file_remove(struct superblock *sb, ino_t inode) {
	struct dev_file *iter = container_of(sb->root, struct dev_file, node);
	struct dev_file *prev = NULL;
	while (iter) {
		if (iter->node.inode == inode)
			break;
		if (iter->node.inode > inode)
			return;
		prev = iter;
		iter = iter->next;
	}

	prev->next = iter->next;

	if (iter->node.mode & VFS_DIR)
		list_destroy(iter->node.private_data);
	kfree(iter);
}

static ssize_t read_blkdev(dev_t dev, void *buf, size_t count, off_t off) {
	ssize_t ret = 0;
	size_t sector_size = get_block_size(dev);
	off_t delta = off % sector_size;
	off -= delta;

	request_t *req = create_request_blkdev(dev, off / sector_size, 0);
	if (!req)
		return -ENOMEM;

	uint32_t nsects = (count + delta) / sector_size;
	if ((count + delta) % sector_size)
		nsects++;

	void *bounce = kmemalign(sector_size, nsects * sector_size);
	if (!bounce) {
		free_request(req);
		return -ENOMEM;
	}

	uintptr_t bounce_offset = 0;
	while (nsects) {
		bio_t *bio = (bio_t *)kmalloc(sizeof(bio_t));
		if (!bio) {
			ret = -ENOMEM;
			goto fail;
		}

		bio->page = resolve_physical((uintptr_t)bounce + bounce_offset);
		bio->offset = bio->page % PAGE_SIZE;
		bio->page = (bio->page / PAGE_SIZE) * PAGE_SIZE;

		if (PAGE_SIZE - bio->offset < nsects * sector_size)
			bio->nsectors = (PAGE_SIZE - bio->offset) / sector_size;
		else
			bio->nsectors = nsects;

		bounce_offset += bio->nsectors * sector_size;
		nsects -= bio->nsectors;

		if ((ret = add_bio_to_request_blkdev(req, bio)) < 0) {
			kfree(bio);
			goto fail;
		}
	}

	if ((ret = post_and_wait_blkdev(req)) < 0)
		goto fail;

	free_request(req);

	memcpy(buf, bounce + delta, count);
	return count;

fail:
	kfree(bounce);
	free_request(req);
	return ret;
}

static ssize_t read(fs_node_t *node, void *buf, size_t count, off_t off) {
	if (node->mode & VFS_CHARDEV) {
		struct chrdev_driver *driver = get_chrdev_driver(MAJOR(node->dev));
		if (driver && driver->ops.read)
			return driver->ops.read(node, buf, count, off);
	} else if (node->mode & VFS_BLOCKDEV)
		return read_blkdev(node->dev, buf, count, off);

	return -EINVAL;
}

static ssize_t write_blkdev(dev_t dev, const void *buf, size_t count, off_t off) {
	ssize_t ret = 0;
	size_t sector_size = get_block_size(dev);
	off_t delta = off % sector_size;
	off -= delta;

	request_t *req = NULL;

	uint32_t nsects = (count + delta) / sector_size;
	if ((count + delta) % sector_size)
		nsects++;

	void *bounce = kmemalign(sector_size, nsects * sector_size);
	if (!bounce)
		return -ENOMEM;

	if (delta) {
		req = create_request_blkdev(dev, off / sector_size, 0);
		if (!req) {
			kfree(bounce);
			return -ENOMEM;
		}

		bio_t *bio = kmalloc(sizeof(bio_t));
		if (!bio) {
			ret = -ENOMEM;
			goto fail;
		}

		bio->page = resolve_physical((uintptr_t)bounce);
		bio->offset = bio->page % PAGE_SIZE;
		bio->page = (bio->page / PAGE_SIZE) * PAGE_SIZE;
		bio->nsectors = 1;

		if ((ret = add_bio_to_request_blkdev(req, bio)) < 0) {
			kfree(bio);
			goto fail;
		}

		if ((ret = post_and_wait_blkdev(req)) < 0)
			goto fail;

		free_request(req);
	}

	if ((count + delta) % sector_size) {
		req = create_request_blkdev(dev, off / sector_size + nsects - 1, 0);
		if (!req) {
			kfree(bounce);
			return -ENOMEM;
		}

		bio_t *bio = kmalloc(sizeof(bio_t));
		if (!bio) {
			ret = -ENOMEM;
			goto fail;
		}

		bio->page = resolve_physical((uintptr_t)bounce +
			(nsects - 1) * sector_size);
		bio->offset = bio->page % PAGE_SIZE;
		bio->page = (bio->page / PAGE_SIZE) * PAGE_SIZE;
		bio->nsectors = 1;

		if ((ret = add_bio_to_request_blkdev(req, bio)) < 0) {
			kfree(bio);
			goto fail;
		}

		if ((ret = post_and_wait_blkdev(req)) < 0)
			goto fail;

		free_request(req);
	}

	memcpy(bounce + delta, buf, count);

	req = create_request_blkdev(dev, off / sector_size, BLOCK_DIR_WRITE);
	if (!req) {
		kfree(bounce);
		return -ENOMEM;
	}

	uintptr_t bounce_offset = 0;
	while (nsects) {
		bio_t *bio = (bio_t *)kmalloc(sizeof(bio_t));
		if (!bio) {
			ret = -ENOMEM;
			goto fail;
		}

		bio->page = resolve_physical((uintptr_t)bounce + bounce_offset);
		bio->offset = bio->page % PAGE_SIZE;
		bio->page = (bio->page / PAGE_SIZE) * PAGE_SIZE;

		if (PAGE_SIZE - bio->offset < nsects * sector_size)
			bio->nsectors = (PAGE_SIZE - bio->offset) / sector_size;
		else
			bio->nsectors = nsects;

		bounce_offset += bio->nsectors * sector_size;
		nsects -= bio->nsectors;

		if ((ret = add_bio_to_request_blkdev(req, bio)) < 0) {
			kfree(bio);
			goto fail;
		}
	}

	if ((ret = post_and_wait_blkdev(req)) < 0)
		goto fail;

	free_request(req);

	return count;

fail:
	kfree(bounce);
	free_request(req);
	return -ENOMEM;
}

static ssize_t write(fs_node_t *node, const void *buf, size_t count, off_t off) {
	if (node->mode & VFS_CHARDEV) {
		struct chrdev_driver *driver = get_chrdev_driver(MAJOR(node->dev));
		if (driver && driver->ops.write)
			return driver->ops.write(node, buf, count, off);
	} else if (node->mode & VFS_BLOCKDEV)
		return write_blkdev(node->dev, buf, count, off);

	return -EINVAL;
}


static int32_t open(fs_node_t *node, uint32_t flags) {
	int32_t ret = 0;

	acquire_semaphore_read((rw_sem_t *)node->fs_sb->private_data);

	fs_node_t *master = get_inode(node->fs_sb, node->inode);
	if (!master || master->nlink == 0) {
		ret = -ENOENT;
		goto exit;
	}

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

exit:
	release_semaphore_read((rw_sem_t *)node->fs_sb->private_data);

	return ret;
}

static int32_t close(fs_node_t *node) {
	int32_t ret = 0;

	acquire_semaphore_write((rw_sem_t *)node->fs_sb->private_data);

	fs_node_t *master = get_inode(node->fs_sb, node->inode);
	if (!master)
		goto exit;

	master->refcount--;

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

exit:
	release_semaphore_write((rw_sem_t *)node->fs_sb->private_data);

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

static int32_t chmod(fs_node_t *node, mode_t mode) {
	fs_node_t *master = get_inode(node->fs_sb, node->inode);
	if (!master)
		return -ENOENT;
	master->mode = (master->mode & VFS_TYPE_MASK) | (mode & VFS_PERM_MASK);
	node->mode = master->mode;

	return 0;
}

static int32_t chown(fs_node_t *node, uid_t uid, gid_t gid) {
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
	struct dev_file *file = container_of(sb->root, struct dev_file, node);
	ino_t inode = 0;
	while (file) {
		if (inode < file->node.inode)
			break;
		inode = file->node.inode + 1;
		file = file->next;
	}

	return inode;
}

static int32_t create(fs_node_t *parent, const char *fname, uid_t uid,
		gid_t gid, mode_t mode, dev_t dev) {
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

	acquire_semaphore_write((rw_sem_t *)parent->fs_sb->private_data);

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
	memcpy(&(new_file->node.ops), &devfs_ops, sizeof(struct file_ops));
	new_file->node.fs_sb = parent->fs_sb;
	if (mode & VFS_DIR) {
		new_file->node.private_data = list_create();
		if (!new_file->node.private_data) {
			kfree(dirp);
			kfree(new_file);
			release_semaphore_write((rw_sem_t *)parent->fs_sb->private_data);
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

	release_semaphore_write((rw_sem_t *)parent->fs_sb->private_data);

	return 0;
}

static int32_t link(fs_node_t *parent, fs_node_t *child, const char *fname) {
	acquire_semaphore_write((rw_sem_t *)parent->fs_sb->private_data);
	fs_node_t *master = get_inode(child->fs_sb, child->inode);
	if (!child) {
		release_semaphore_write((rw_sem_t*)parent->fs_sb->private_data);
		return -ENOTDIR;
	}

	struct dirent *dirp = (struct dirent *)kmalloc(sizeof(struct dirent));
	if (!dirp) {
		release_semaphore_write((rw_sem_t*)parent->fs_sb->private_data);
		return -ENOMEM;
	}
	dirp->d_ino = child->inode;
	strcpy(dirp->d_name, fname);

	child->nlink = ++(master->nlink);
	list_insert((list_t *)parent->private_data, dirp);

	release_semaphore_write((rw_sem_t*)parent->fs_sb->private_data);

	return 0;
}

static int32_t unlink(fs_node_t *parent, const char *fname) {
	acquire_semaphore_write((rw_sem_t *)parent->fs_sb->private_data);
	fs_node_t *master = get_inode(parent->fs_sb, parent->inode);
	if (!master) {
		release_semaphore_write((rw_sem_t*)parent->fs_sb->private_data);
		return -ENOTDIR;
	}

	node_t *iter;
	foreach(iter, ((list_t *)parent->private_data)) {
		if (strcmp(((struct dirent *)iter->data)->d_name, fname) == 0)
			break;
	}

	if (!iter) {
		release_semaphore_write((rw_sem_t*)parent->fs_sb->private_data);
		return -ENOENT;
	}

	fs_node_t *child = get_inode(parent->fs_sb, 
		((struct dirent *)iter->data)->d_ino);
	if (!child) {// This... would not be expected
		release_semaphore_write((rw_sem_t*)parent->fs_sb->private_data);
		return -ENOENT;
	}

	list_remove((list_t *)parent->private_data, iter);

	if (--child->nlink == 0 && child->refcount == 0)
		file_remove(parent->fs_sb, child->inode);

	release_semaphore_write((rw_sem_t*)parent->fs_sb->private_data);

	return 0;
}