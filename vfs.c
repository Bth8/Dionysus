/* vfs.c - VFS function implementation */
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

// Reader, beware. It gets a bit kludgy

#include <vfs.h>
#include <common.h>
#include <string.h>
#include <kmalloc.h>

fs_node_t *vfs_root = NULL;
struct file_system_type *fs_types = NULL;

fs_node_t mnt_pts[MAX_MNT_PTS];
u32int nmnts = 0;

u32int read_vfs(fs_node_t *node, void *buf, u32int count, u32int off) {
	if (!node)
		return 0;
	if (node->ops.read)
		return node->ops.read(node, buf, count, off);
	else
		return 0;
}

u32int write_vfs(fs_node_t *node, const void *buf, u32int count, u32int off) {
	if (!node)
		return 0;
	if (node->ops.write)
		return node->ops.write(node, buf, count, off);
	else
		return 0;
}

void open_vfs(fs_node_t *node, u32int flags) {
	if (!node)
		return;
	if (node->ops.open)
		node->ops.open(node, flags);
}

void close_vfs(fs_node_t *node) {
	if (!node)
		return;
	if (node == vfs_root)
		PANIC("Tried closing root");

	if (node->ops.close)
		node->ops.close(node);
}

// Determines if the fs node passed to it is a mountpoint, returns the root if it is
static fs_node_t *get_mnt(fs_node_t *node) {
	u32int i;
	for (i = 0; i < nmnts; i++) {
		if (node->fs_sb != mnt_pts[i].fs_sb)
			continue;
		if (node->inode != mnt_pts[i].inode)
			continue;
		break;
	}
	if (i != nmnts)
		return mnt_pts[i].ptr_sb->root;

	return node;
}

struct dirent *readdir_vfs(fs_node_t *node, u32int index) {
	if (!node)
		return NULL;
	if (!(node->flags & VFS_DIR))
		return NULL;
	fs_node_t *mnt = get_mnt(node);
	if (mnt->ops.readdir)
		return mnt->ops.readdir(mnt, index);
	return NULL;
}

fs_node_t *finddir_vfs(fs_node_t *node, const char *name) {
	if (!node)
		return NULL;
	if (!(node->flags & VFS_DIR))
		return NULL;
	fs_node_t *mnt = get_mnt(node);
	if (mnt->ops.finddir)
		return mnt->ops.finddir(mnt, name);
	else
		return NULL;
}

s32int ioctl_vfs(fs_node_t *node, u32int request, void *ptr) {
	if (!node)
		return -1;
	if (!(node->flags & VFS_CHARDEV) && !(node->flags & VFS_BLOCKDEV))
		return -1;
	if (node->ops.ioctl)
		return node->ops.ioctl(node, request, ptr);
	return -1;
}

s32int register_fs(struct file_system_type *fs) {
	struct file_system_type *fsi = fs_types;
	if (fsi == NULL) {
		fs_types = fs;
		return 0;
	}
	for (; fsi->next != NULL; fsi = fsi->next)
		if (strcmp(fsi->name, fs->name) == 0)
			return -1;

	fsi->next = fs;
	return 0;
}

// TODO: Return error codes slightly more useful than -1
s32int mount(fs_node_t *dev, fs_node_t *dest, const char *fs_name, u32int flags) {
	struct file_system_type *fsi = fs_types;
	struct superblock *sb = NULL;
	if (nmnts >= MAX_MNT_PTS)
		return -1;
	if (!(dest->flags & VFS_DIR))
		return -1;
	if (dest == NULL && vfs_root != NULL)
		return -1;
	if (fsi == NULL)
		return -1;
	for (; fsi->next != NULL; fsi = fsi->next)
		if (strcmp(fsi->name, fs_name) == 0)
			break;
	if (fsi == NULL)
		return -1;
	if (fsi->flags & FS_NODEV) {
		if (dev)
			return -1;
	} else {
		if (!dev)
			return -1;
	}

	if ((sb = fsi->get_super(flags, dev)) != NULL) {
		if (dest == vfs_root) {
			vfs_root = sb->root;
			vfs_root->ptr_sb = sb;
		} else {
			dest->flags |= VFS_MOUNT;
			dest->ptr_sb = sb;
			memcpy(&mnt_pts[nmnts++], dest, sizeof(fs_node_t));
		}
		return 0;
	}
	return -1;
}

// Traverses the path to get the correct file
fs_node_t *get_path(const char *path) {
	// Sanity check
	if (!path || path[0] != '/')
		return NULL;

	int path_len = strlen(path);
	if (path_len == 1) {								// It's just root
		fs_node_t *ret = kmalloc(sizeof(fs_node_t));
		memcpy(ret, vfs_root, sizeof(fs_node_t));
		return ret;
	}

	char *path_cpy = (char *)kmalloc(path_len + 1);
	strcpy(path_cpy, path);
	char *off;
	int depth = 0;

	// Breaks the path into several strings with / as its delimiter
	for (off = path_cpy; off < path_cpy + path_len; off++)
		if (*off == '/') {
			*off = '\0';
			depth++;
		}

	off = path_cpy + 1;
	fs_node_t *cur_node = (fs_node_t *)kmalloc(sizeof(fs_node_t));
	memcpy(cur_node, vfs_root, sizeof(fs_node_t));
	fs_node_t *next_node;
	int i;
	for (i = 0; i < depth; i++) {
		next_node = finddir_vfs(cur_node, off);
		kfree(cur_node);
		cur_node = next_node;
		if (!cur_node) {
			kfree(path_cpy);
			return NULL;
		} else if (i == depth - 1) {
			kfree(path_cpy);
			return cur_node;
		}
		off += strlen(off) + 1;
	}

	// Shouldn't be reached
	kfree(path_cpy);
	return NULL;
}

fs_node_t *create_vfs(const char *path, u32int uid, u32int gid, u32int mode) {
	if (!path || path[0] != '/')
		return NULL;

	int i = strlen(path);
	// Just passed '/'
	if (i == 1)
		return NULL;

	// Ensure no trailing '/'
	if (path[i - 1] == '/')
		return NULL;

	char *parent_path = kmalloc(i + 1);
	strcpy(parent_path, path);

	// Create the parent directory
	for ( i -= 1; i >= 0; i--)
		if (parent_path[i] == '/') {
			parent_path[i] = '\0';
			break;
		}

	fs_node_t *parent;
	if (i >= 0)
		parent = get_path(parent_path);
	else
		parent = get_path("/");

	kfree(parent_path);

	fs_node_t *node = NULL;
	if (parent && (parent->flags & VFS_DIR) && parent->ops.create)
		node = parent->ops.create(parent, path + i + 1, uid, gid, mode);

	kfree(parent);
	return node;
}
