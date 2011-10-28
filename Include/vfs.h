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

#define VFS_O_EXEC		00001
#define VFS_O_WRITE		00002
#define VFS_O_READ		00004
#define VFS_G_EXEC		00010
#define VFS_G_WRITE		00020
#define VFS_G_READ		00040
#define VFS_U_EXEC		00100
#define VFS_U_WRITE		00200
#define VFS_U_READ		00400
#define VFS_STICKY		01000
#define VFS_SETGID		02000
#define VFS_SETUID		04000

#define EOF				-1

struct fs_node;
struct dirent {
	u32int d_ino;
	char d_name[NAME_MAX];
};

struct file_ops {
	u32int(*read)(struct fs_node*, char*, u32int, u32int);
	u32int(*write)(struct fs_node*, const char*, u32int, u32int);
	void(*open)(struct fs_node*, u32int);
	void(*close)(struct fs_node*);
	struct dirent*(*readdir)(struct fs_node*, u32int);
	struct fs_node*(*finddir)(struct fs_node*, const char*);
	s32int(*fsync)(struct fs_node*);
};

typedef struct fs_node {
	char name[NAME_MAX];
	u32int mask;			// Permissions mask
	u32int gid;
	u32int uid;
	u32int flags;			// Includes node type
	u32int inode;			// Way for individual FSs to differentiate between files
	u32int len;
	u32int impl;			// Implementation-defined
	struct file_ops ops;
	struct fs_node *ptr;	// Used for syms/mounts
} fs_node_t;

extern fs_node_t *vfs_root;

u32int read_vfs(fs_node_t *node, char *buf, u32int count, u32int off);
u32int write_vfs(fs_node_t *node, const char *buf, u32int count, u32int off);
void open_vfs(fs_node_t *node, u32int flags);
void close_vfs(fs_node_t *node);
struct dirent *readdir_vfs(fs_node_t *node, u32int index);
fs_node_t *finddir_vfs(fs_node_t *node, const char *name);
fs_node_t *kopen(const char *path, u32int flags);
u8int mount(fs_node_t *src, fs_node_t *target, const void *data);

#endif
