/* vfs.c - VFS function implementation */
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

#include <vfs.h>
#include <common.h>
#include <string.h>
#include <kmalloc.h>

fs_node_t *vfs_root = NULL;
struct file_system_type *fs_types = NULL;

u32int read_vfs(fs_node_t *node, char *buf, u32int count, u32int off) {
	if (node->ops.read)
		return node->ops.read(node, buf, count, off);
	else
		return 0;
}

u32int write_vfs(fs_node_t *node, const char *buf, u32int count, u32int off) {
	if (node->ops.write)
		return node->ops.write(node, buf, count, off);
	else
		return 0;
}

void open_vfs(fs_node_t *node, u32int flags) {
	if (node->ops.open)
		node->ops.open(node, flags);
}

void close_vfs(fs_node_t *node) {
	if (node == vfs_root)
		PANIC("Tried closing root");

	if (node->ops.close)
		node->ops.close(node);
}

struct dirent *readdir_vfs(fs_node_t *node, u32int index) {
	if ((node->flags & VFS_MOUNT) && node->ptr->ops.readdir)
		return node->ptr->ops.readdir(node->ptr, index);
	else if ((node->flags & VFS_DIR) && node->ops.readdir)
		return node->ops.readdir(node, index);
	else
		return NULL;
}

fs_node_t *finddir_vfs(fs_node_t *node, const char *name) {
	if ((node->flags & VFS_MOUNT) && node->ptr->ops.finddir)
		return node->ptr->ops.finddir(node->ptr, name);
	else if ((node->flags & VFS_DIR) && node->ops.finddir)
		return node->ops.finddir(node, name);
	else
		return NULL;
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

s32int mount(fs_node_t *dev, fs_node_t *dest, const char *fs_name, u32int flags) {
	struct file_system_type *fsi = fs_types;
	struct superblock *sb = NULL;
	if (dest == NULL)
		return -1;
	if (fsi == NULL)
		return -1;
	for (; fsi->next != NULL; fsi = fsi->next)
		if (strcmp(fsi->name, fs_name) == 0)
			break;
	if (fsi->next == NULL)
		return -1;

	if ((sb = fsi->get_super(fsi, flags, dev)) != NULL) {
		dest->flags &= VFS_MOUNT;
		dest->ptr = sb->root;
		dest->sb = sb;
		return 0;
	} else
		return -1;
}

fs_node_t *kopen(const char *path, u32int flags) {
	if (!path || path[0] != '/')
		return NULL;

	int path_len = strlen(path);
	if (path_len == 1) {
		fs_node_t *ret = kmalloc(sizeof(fs_node_t));
		memcpy(ret, vfs_root, sizeof(fs_node_t));
		return ret;
	}

	char *path_cpy = (char *)kmalloc(path_len + 1);
	memcpy(path_cpy, path, sizeof(path_cpy));
	char *off;
	int depth = 0;

	for (off = path_cpy; off < path_cpy + path_len; off++)
		if (*off == '/') {
			*off = '\0';
			depth++;
		}

	path_cpy[path_len] = '\0';
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
			open_vfs(cur_node, flags);
			kfree(path_cpy);
			return cur_node;
		}
		off += strlen(off) + 1;
	}

	kfree(path_cpy);
	return NULL;
}
