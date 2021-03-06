/* vfs.c - VFS function implementation */

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

// Reader, beware. It gets a bit kludgy

#include <vfs.h>
#include <common.h>
#include <string.h>
#include <kmalloc.h>
#include <errno.h>
#include <structures/hashmap.h>
#include <structures/list.h>
#include <structures/tree.h>
#include <task.h>
#include <block.h>

fs_node_t *vfs_root = NULL;
hashmap_t *fs_types = NULL;
tree_t *filesystem = NULL;

extern volatile task_t *current_task;

volatile spinlock_t vfs_lock = 0;
volatile spinlock_t refcount_lock = 0;

void init_vfs(void) {
	ASSERT(!filesystem && !fs_types && "Double initialization");

	filesystem = tree_create();
	ASSERT(filesystem);

	fs_types = hashmap_create(32, NULL);
	ASSERT(fs_types);
}

// tokenizes path in place, returns depth
static uint32_t vfs_tokenize(char *path) {
	ASSERT(path);

	uint32_t len = strlen(path);
	char *off;
	uint32_t depth = 1;

	for (off = path; off < path + len + 1; off++)
		if (*off == PATH_DELIMITER) {
			*off = '\0';
			depth++;
		}

	return depth;
}

// Converts relative paths to absolute. Leaves absolute paths alone
char *canonicalize_path(const char *cwd, const char *relpath) {
	ASSERT(relpath && cwd);

	list_t *fifo = list_create();
	if (!fifo)
		return NULL;

	// relative path
	if (relpath[0] != PATH_DELIMITER) {
		char *cwd_cpy = (char *)kmalloc(strlen(cwd) + 1);
		if (!cwd_cpy)
			goto error;

		char *off = cwd_cpy;
		strcpy(cwd_cpy, cwd);
		uint32_t depth;
		for (depth = vfs_tokenize(cwd_cpy); depth > 0; depth--) {
			if (*off == '\0') {
				off++;
				continue;
			}
			char *s = (char *)kmalloc(strlen(off) + 1);
			if (!s) {
				kfree(cwd_cpy);
				goto error;
			}

			strcpy(s, off);
			list_insert(fifo, s);
			off += strlen(off) + 1;
		}
		kfree(cwd_cpy);
	}

	char *path_cpy = (char *)kmalloc(strlen(relpath) + 1);
	if (!path_cpy)
		goto error;

	char *off = path_cpy;
	strcpy(path_cpy, relpath);
	uint32_t depth;
	node_t *prev = fifo->tail;
	for (depth = vfs_tokenize(path_cpy); depth > 0; depth--) {
		if (*off == '\0') {
			off++;
			continue;
		}
		if (strcmp(off, PATH_THIS_DIR) == 0) {
			off += 2;
			continue;
		}
		if (strcmp(off, PATH_PARENT_DIR) == 0) {
			list_remove(fifo, prev);
			off += 3;
			continue;
		}
		char *s = (char *)kmalloc(strlen(off) + 1);
		if (!s) {
			kfree(path_cpy);
			goto error;
		}

		strcpy(s, off);
		prev = list_insert(fifo, s);
		off += strlen(off) + 1;
	}
	kfree(path_cpy);

	node_t *element;
	size_t size = 0;
	foreach(element, fifo) {
		size += strlen((char *)element->data) + 1;
	}

	char *canonical = (char *)kmalloc(size + 1);
	off = canonical;
	if (!canonical)
		goto error;

	if (size == 0) {
		if (krealloc(canonical, 2) == NULL) {
			kfree(canonical);
			goto error;
		}
		canonical[0] = PATH_DELIMITER;
		canonical[1] = '\0';
	} else
		foreach(element, fifo) {
			*(off++) = PATH_DELIMITER;
			strcpy(off, element->data);
			off += strlen((char *)element->data);
		}

	list_destroy(fifo);
	return canonical;

error:
	list_destroy(fifo);
	return NULL;
}

ssize_t read_vfs(fs_node_t *node, void *buf, size_t count, off_t off) {
	if (!node)
		return -EBADF;
	if (node->mode & VFS_DIR)
		return -EINVAL;
	if (node->ops.read)
		return node->ops.read(node, buf, count, off);
	else
		return -1;
}

ssize_t write_vfs(fs_node_t *node, const void *buf, size_t count, off_t off) {
	if (!node)
		return -EBADF;
	if (node->mode & VFS_DIR)
		return -EINVAL;
	if (node->ops.write)
		return node->ops.write(node, buf, count, off);
	else
		return -1;
}

int32_t open_vfs(fs_node_t *node, uint32_t flags) {
	if (!node)
		return -EBADF;

	if ((node->fs_sb->flags & MNT_RDONLY) && flags & O_WRONLY)
		return -EROFS;

	node->flags = flags;

	if (node->refcount == -1)
		return 0;

	spin_lock(&refcount_lock);
	int32_t ret = -1;
	if (node->ops.open)
		ret = node->ops.open(node, flags);
	else
		ret = -EACCES;

	if (node->refcount >= 0 && ret >= 0) {
		node->refcount++;
	}
	spin_unlock(&refcount_lock);

	return ret;
}

int32_t close_vfs(fs_node_t *node) {
	if (!node)
		return -EBADF;

	if (node->refcount == -1) {
		kfree(node);
		return 0;
	}

	spin_lock(&refcount_lock);

	node->refcount--;
	int ret = 0;
	if (node->refcount == 0) {
		if (node->ops.close)
			ret = node->ops.close(node);

		kfree(node);
	}

	spin_unlock(&refcount_lock);

	return ret;
}

int32_t readdir_vfs(fs_node_t *node, struct dirent *dirp, uint32_t index) {
	if (!node)
		return -EBADF;
	if (!(node->mode & VFS_DIR))
		return -ENOTDIR;
	if (node->ops.readdir)
		return node->ops.readdir(node, dirp, index);
	return -1;
}

static fs_node_t *finddir_vfs(fs_node_t *node, const char *name) {
	if (!node)
		return NULL;
	if (!(node->mode & VFS_DIR))
		return NULL;
	if (node->ops.finddir)
		return node->ops.finddir(node, name);
	else
		return NULL;
}

int32_t stat_vfs(fs_node_t *node, struct stat *buff) {
	ASSERT(buff);
	if (!node)
		return -EBADF;

	buff->st_dev = node->fs_sb->dev;
	buff->st_ino = node->inode;
	buff->st_mode = node->mode;
	buff->st_nlink = node->nlink;
	buff->st_uid = node->uid;
	buff->st_gid = node->gid;
	buff->st_rdev = node->dev;
	buff->st_size = node->len;
	buff->st_atime = node->atime;
	buff->st_mtime = node->mtime;
	buff->st_ctime = node->ctime;
	buff->st_blksize = node->fs_sb->blocksize;
	buff->st_blocks = buff->st_size / buff->st_blksize;

	return 0;
}

int32_t chmod_vfs(fs_node_t *node, mode_t mode) {
	if (!node)
		return -EBADF;

	if (node->ops.chmod)
		return node->ops.chmod(node, mode);
	return 0;
}

int32_t chown_vfs(fs_node_t *node, uid_t uid, gid_t gid) {
	if (!node)
		return -EBADF;

	if (node->ops.chmod)
		return node->ops.chown(node, uid, gid);
	return 0;
}

int32_t ioctl_vfs(fs_node_t *node, uint32_t req, void *data) {
	if (!node)
		return -EBADF;
	if (!(node->mode & (VFS_CHARDEV | VFS_BLOCKDEV)))
		return -ENOTTY;
	if (node->ops.ioctl)
		return node->ops.ioctl(node, req, data);
	return -EINVAL;
}

int32_t create_vfs(fs_node_t *parent, const char *fname, uid_t uid, 
		gid_t gid, mode_t mode, dev_t dev) {
	if (!parent)
		return -EBADF;
	if (!(parent->mode & VFS_DIR))
		return -ENOTDIR;
	fs_node_t *exist = finddir_vfs(parent, fname);
	if (exist) {
		kfree(exist);
		return -EEXIST;
	}
	if (parent->ops.create)
		return parent->ops.create(parent, fname, uid, gid, mode, dev);
	return -EACCES;
}

int32_t link_vfs(fs_node_t *parent, fs_node_t *child, const char *fname) {
	if (!child || !parent)
		return -EBADF;
	if (!(parent->mode & VFS_DIR))
		return -ENOTDIR;
	if (parent->fs_sb != child->fs_sb)
		return -EXDEV;
	fs_node_t *exist = finddir_vfs(parent, fname);
	if (exist) {
		kfree(exist);
		return -EEXIST;
	}
	if (parent->ops.link)
		return parent->ops.link(parent, child, fname);
	return -EPERM;
}

int32_t unlink_vfs(fs_node_t *parent, const char *fname) {
	if (!parent)
		return -EBADF;
	if (!(parent->mode & VFS_DIR))
		return -ENOTDIR;
	if (parent->ops.unlink)
		return parent->ops.unlink(parent, fname);
	return -EACCES;
}

int32_t register_fs(const char *name, struct file_system_type *fs) {
	return hashmap_insert(fs_types, name, fs);
}

static void vfs_prune(tree_node_t *node) {
	ASSERT(node && !(((struct mountpoint *)(node->data))->sb));
	tree_node_t *branch_head = NULL;

	while (1) {
		// more than one child, we can't delete it
		if (node->children->head != node->children->tail)
			break;
		// Also can't delete if it's an active mountpoint
		if (((struct mountpoint *)(node->data))->sb)
			break;
		// We're freeing the root node?
		if (node == NULL)
			break;

		branch_head = node;
		node = node->parent;
	}

	tree_delete_branch(filesystem, branch_head);
}

int32_t mount(fs_node_t *dev, const char *relpath, const char *fs_name,
		uint32_t flags) {
	if (!filesystem)
		return -EACCES;

	if (!fs_types)
		return -ENOENT;

	file_system_t *fs = hashmap_find(fs_types, fs_name);
	if (!fs)
		return -ENODEV;

	if (!dev && !(fs->flags & FS_NODEV))
		return -ENODEV;

	if (dev && !(dev->mode & VFS_BLOCKDEV))
		return -ENOTBLK;

	char *path = canonicalize_path(current_task->cwd, relpath);
	if (!path) {
		spin_unlock(&vfs_lock);
		return -ENOMEM;
	}

	uint32_t depth = vfs_tokenize(path);

	spin_lock(&vfs_lock);

	if (!filesystem->root) {
		struct mountpoint *root =
			(struct mountpoint *)kmalloc(sizeof(struct mountpoint));
		if (!root)
			return -ENOMEM;

		root->name = "[root]";
		root->sb = NULL;
		if (!tree_set_root(filesystem, root)) {
			kfree(root);
			return -ENOMEM;
		}
	}

	char *off;
	struct mountpoint *entry = NULL;
	tree_node_t *node = filesystem->root;

	for (off = path; depth > 0; depth--) {
		if (strlen(off) == 0) {
			off++;
			continue;
		}
		node_t *child;
		int exist = 0;
		foreach(child, node->children) {
			entry = (struct mountpoint *)((tree_node_t *)child->data)->data;
			if (strcmp(entry->name, off) == 0) {
				exist = 1;
				node = (tree_node_t *)child->data;
				break;
			}
		}
		if (!exist) {
			entry = (struct mountpoint *)kmalloc(sizeof(struct mountpoint));
			if (!entry) {
				kfree(path);
				vfs_prune(node);
				spin_unlock(&vfs_lock);
				return -ENOMEM;
			}
			entry->name = (char *)kmalloc(strlen(off) + 1);
			if (!entry->name) {
				kfree(entry);
				kfree(path);
				vfs_prune(node);
				spin_unlock(&vfs_lock);
				return -ENOMEM;
			}
			strcpy(entry->name, off);
			entry->sb = NULL;
			node = tree_insert_node(filesystem, node, entry);
		}
		off += strlen(off) + 1;
	}

	kfree(path);

	if (entry->sb) {
		vfs_prune(node);
		spin_unlock(&vfs_lock);
		return -EBUSY;
	}

	struct superblock *sb = NULL;
	if (dev)
		sb = fs->get_super(dev->dev, flags);
	else
		sb = fs->get_super(0, flags);
	if (!sb) {
		spin_unlock(&vfs_lock);
		vfs_prune(node);
		return -ENODEV;
	}

	sb->root->refcount = -1;
	entry->sb = sb;
	spin_unlock(&vfs_lock);
	return 0;
}


int32_t umount(const char *relpath, uint32_t flags) {
	if (!filesystem)
		return -EINVAL;
	
	char *path = canonicalize_path(current_task->cwd, relpath);
	if (!path)
		return -ENOMEM;

	char *off;
	uint32_t depth = vfs_tokenize(path);

	spin_lock(&vfs_lock);
	tree_node_t *node = filesystem->root;
	struct mountpoint *entry = (struct mountpoint *)node->data;

	for (off = path; depth > 0; depth--) {
		if (strlen(off) == 0) {
			off++;
			continue;
		}

		node_t *child;
		int exist = 0;
		foreach(child, node->children) {
			entry = (struct mountpoint *)((tree_node_t *)child->data)->data;
			if (strcmp(entry->name, off) == 0) {
				exist = 1;
				node = (tree_node_t *)child->data;
				break;
			}
		}

		if (!exist) {
			kfree(path);
			spin_unlock(&vfs_lock);
			return -ENOENT;
		}
		off += strlen(off);
	}

	kfree(path);

	if (!entry->sb) {
		spin_unlock(&vfs_lock);
		return -EINVAL;
	}

	if (!(flags & MNT_DETACH) && node->children->head != node->children->tail) {
		spin_unlock(&vfs_lock);
		return -EBUSY;
	}


	if (entry->sb->close_fs(entry->sb, flags & MNT_FORCE) != 0) {
		spin_unlock(&vfs_lock);
		return -EBUSY;
	}

	entry->sb = NULL;
	vfs_prune(node);
	spin_unlock(&vfs_lock);
	return 0;
}

static fs_node_t *get_local_root(char **path, uint32_t *path_depth) {
	tree_node_t *node = filesystem->root;
	fs_node_t *local_root = ((struct mountpoint *)node->data)->sb->root;
	uint32_t final_depth = 0;
	char *off = *path;
	char *final_off = off;

	uint32_t depth;
	for (depth = 1; depth <= *path_depth; depth++) {
		if (strlen(off) == 0) {
			off++;
			continue;
		}
		int exist = 0;
		node_t *child;
		foreach (child, node->children) {
			struct mountpoint *entry =
				(struct mountpoint *)((tree_node_t *)child->data)->data;
			if (strcmp(off, entry->name) == 0) {
				exist = 1;
				node = (tree_node_t *)child->data;
				if (entry->sb) {
					final_depth = depth;
					final_off = off + strlen(off) + 1;
					local_root = entry->sb->root;
				}
				break;
			}
		}
		if (!exist)
			break;
		off += strlen(off) + 1;
	}

	*path = final_off;
	*path_depth -= final_depth;

	if (local_root) {
		fs_node_t *ret = (fs_node_t *)kmalloc(sizeof(fs_node_t));
		if (!ret)
			return NULL;
		memcpy(ret, local_root, sizeof(fs_node_t));
		return ret;
	}
	return NULL;
}

// Traverses the path to get the correct file
fs_node_t *kopen(const char *relpath, int32_t flags, int32_t *openret) {
	// Sanity check
	if (!relpath)
		return NULL;

	char *path = canonicalize_path(current_task->cwd, relpath);
	if (!path)
		return NULL;

	uint32_t depth = vfs_tokenize(path);
	char *off = path;

	spin_lock(&vfs_lock);

	if (depth == 1) {
		fs_node_t *root = (fs_node_t *)kmalloc(sizeof(fs_node_t));
		memcpy(root, (fs_node_t *)filesystem->root->data, sizeof(fs_node_t));
		kfree(path);
		int32_t ret = open_vfs(root, flags);
		spin_unlock(&vfs_lock);
		if (ret < 0) {
			kfree(root);
			root = NULL;
		}
		if (openret)
			*openret = ret;
		return root;
	}

	fs_node_t *cur_node = get_local_root(&off, &depth);
	fs_node_t *next_node;

	if (!cur_node) {
		kfree(path);
		spin_unlock(&vfs_lock);
		if (openret)
			*openret = -ENOENT;
		return NULL;
	}

	if (depth == 0) {
		kfree(path);
		int32_t ret = open_vfs(cur_node, flags);
		if (ret < 0) {
			kfree(cur_node);
			cur_node = NULL;
		}
		spin_unlock(&vfs_lock);
		if (openret)
			*openret = ret;
		return cur_node;
	}

	for (; depth > 0; depth--) {
		next_node = finddir_vfs(cur_node, off);
		kfree(cur_node);
		cur_node = next_node;
		if (!cur_node) {
			kfree(path);
			spin_unlock(&vfs_lock);
			if (openret)
				*openret = -ENOENT;
			return NULL;
		} else if (depth == 1) {
			kfree(path);
			int32_t ret = open_vfs(cur_node, flags);
			if (ret < 0) {
				kfree(cur_node);
				cur_node = NULL;
			}
			spin_unlock(&vfs_lock);
			if (openret)
				*openret = ret;
			return cur_node;
		}
		off += strlen(off) + 1;
	}

	kfree(path);
	spin_unlock(&vfs_lock);
	return NULL;
}

fs_node_t *clone_file(fs_node_t *node) {
	if (!node)
		return NULL;

	if (node->refcount == -1) {
		fs_node_t *node_cpy = (fs_node_t *)kmalloc(sizeof(fs_node_t));
		if (node_cpy)
			memcpy(node_cpy, node, sizeof(fs_node_t));

		return node_cpy;
	}

	spin_lock(&refcount_lock);
	if (node->refcount >= 0) {
		node->refcount++;
	}
	spin_unlock(&refcount_lock);

	return node;
}