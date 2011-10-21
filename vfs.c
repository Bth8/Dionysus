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

u32int read_vfs(fs_node_t *node, char *buf, u32int count, u32int off) {
	if (node->read)
		return node->read(node, buf, count, off);
	else
		return 0;
}

u32int write_vfs(fs_node_t *node, const char *buf, u32int count, u32int off) {
	if (node->write)
		return node->write(node, buf, count, off);
	else
		return 0;
}

void open_vfs(fs_node_t *node, u32int flags) {
	if (node->open)
		node->open(node, flags);
}

void close_vfs(fs_node_t *node) {
	if (node == vfs_root)
		PANIC("Tried closing root");

	if (node->close)
		node->close(node);
}

struct dirent *readdir_vfs(fs_node_t *node, u32int index) {
	if ((node->flags & VFS_MOUNT) && node->ptr->readdir)
		return node->ptr->readdir(node->ptr, index);
	else if ((node->flags & VFS_DIR) && node->readdir)
		return node->readdir(node, index);
	else
		return NULL;
}

fs_node_t *finddir_vfs(fs_node_t *node, const char *name) {
	if ((node->flags & VFS_MOUNT) && node->ptr->finddir)
		return node->ptr->finddir(node->ptr, name);
	else if ((node->flags & VFS_DIR) && node->finddir)
		return node->finddir(node, name);
	else
		return NULL;
}

u8int mount(struct mountpoint *mnt) {
	if (!(mnt->point->flags & VFS_DIR))
		return 1;
	if (!mnt->init || mnt->init(mnt->root) != 0)
		return 2;

	mnt->point->flags |= VFS_MOUNT;
	mnt->point->ptr = mnt->root;

	return 0;
}

fs_node_t *kopen(const char *path, u32int flags) {
	if (!vfs_root || !path || path[0] != '/')
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
