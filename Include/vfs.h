/* vfs.h - basic VFS functions */
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

#ifndef VFS_H
#define VFS_H
#include <common.h>

#define NAME_MAX		256

#define VFS_FILE		0x01
#define VFS_DIR			0x02
#define VFS_CHARDEV		0x04
#define VFS_BLOCKDEV	0x08
#define VFS_PIPE		0x10
#define VFS_MOUNT		0x20
#define VFS_SYM			0x40

struct fs_node;
struct dirent {
	u32int d_ino;
	char d_name[NAME_MAX];
};

typedef u32int(*read_type_t)(struct fs_node*, char*, u32int, u32int);
typedef u32int(*write_type_t)(struct fs_node*, const char*, u32int, u32int);
typedef void(*open_type_t)(struct fs_node*, u32int);
typedef void(*close_type_t)(struct fs_node*);
typedef struct dirent*(*readdir_type_t)(struct fs_node*, u32int);
typedef struct fs_node*(*finddir_type_t)(struct fs_node*, const char*);

typedef u8int(*init_fs)(struct fs_node *);

typedef struct fs_node {
	char name[NAME_MAX];
	u32int mask;			// Permissions mask
	u32int gid;
	u32int uid;
	u32int flags;			// Includes node type
	u32int inode;			// Way for individual FSs to differentiate between files
	u32int len;
	u32int impl;			// Implementation-defined
	read_type_t read;
	write_type_t write;
	open_type_t open;
	close_type_t close;
	readdir_type_t readdir;
	finddir_type_t finddir;
	struct fs_node *ptr;	// Used for syms/mounts
} fs_node_t;

struct mountpoint {
	fs_node_t *point;
	fs_node_t *root;
	init_fs init;
};

extern fs_node_t *vfs_root;

u32int read_vfs(fs_node_t *node, char *buf, u32int count, u32int off);
u32int write_vfs(fs_node_t *node, const char *buf, u32int count, u32int off);
void open_vfs(fs_node_t *node, u32int flags);
void close_vfs(fs_node_t *node);
struct dirent *readdir_vfs(fs_node_t *node, u32int index);
fs_node_t *finddir_vfs(fs_node_t *node, const char *name);
fs_node_t *kopen(const char *path, u32int flags);
u8int mount(struct mountpoint *mnt);

#endif
