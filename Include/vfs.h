/* vfs.h - basic VFS functions */
/* Copyright (C) 2011-2013 Bth8 <bth8fwd@gmail.com>
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
#include <time.h>

#define NAME_MAX		256
#define MAX_MNT_PTS		32

#define VFS_FILE		0x01
#define VFS_DIR			0x02
#define VFS_CHARDEV		0x04
#define VFS_BLOCKDEV	0x08
#define VFS_PIPE		0x10
#define VFS_MOUNT		0x20
#define VFS_LINK		0x40
#define VFS_UNKNOWN		0x80

#define O_RDONLY		0x01
#define O_WRONLY		0x02
#define O_RDWR			(O_RDONLY | O_WRONLY)
#define O_APPEND		0x04
#define O_CREAT			0x08
#define O_EXCL			0x10
#define O_TRUNC			0x20

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

#define FS_NODEV		0x01

#define EOF				-1

struct fs_node;
struct stat;
struct dirent {
	u32int d_ino;
	char d_name[NAME_MAX];
};

struct file_ops {
	u32int(*read)(struct fs_node*, void*, size_t, off_t);
	u32int(*write)(struct fs_node*, const void*, size_t, off_t);
	int(*open)(struct fs_node*, u32int);
	int(*close)(struct fs_node*);
	struct fs_node*(*create)(struct fs_node*, const char*, u32int, u32int, u32int);
	int(*readdir)(struct fs_node*, struct dirent*, u32int);
	struct fs_node*(*finddir)(struct fs_node*, const char*);
	int (*stat)(struct fs_node*, struct stat*);
	s32int (*ioctl)(struct fs_node*, u32int, void*);
};

typedef struct fs_node {
	char name[NAME_MAX];
	u32int mask;				// Permissions mask
	int gid;
	int uid;
	u32int flags;				// Includes node type
	u32int inode;				// Way for individual FSs to differentiate between files
	size_t len;
	u32int impl;				// Implementation-defined
	struct file_ops ops;
	struct superblock *fs_sb;	// Parent fs
	struct superblock *ptr_sb;	// For mount points
	void *private_data;
} fs_node_t;

struct file_system_type;
struct superblock {
	fs_node_t *dev;
	void *private_data;
	fs_node_t *root;
	u32int flags;
};

struct file_system_type {
	const char *name;
	u32int flags;
	struct superblock *(*get_super)(u32int, fs_node_t*);
	struct file_system_type *next;
};

struct stat {
	u32int st_dev;
	u32int st_ino;
	u32int st_mode;
	u32int st_nlink;
	u32int uid;
	u32int gid;
	u32int st_rdev;
	size_t st_size;
	size_t st_blksize;
	u32int st_blocks;
	time_t st_atime;
	time_t st_mtime;
	time_t st_ctime;
};

extern fs_node_t *vfs_root;

u32int read_vfs(fs_node_t *node, void *buf, size_t count, off_t off);
u32int write_vfs(fs_node_t *node, const void *buf, size_t count, off_t off);
int open_vfs(fs_node_t *node, u32int flags);
int close_vfs(fs_node_t *node);
int readdir_vfs(fs_node_t *node, struct dirent *dirp, u32int index);
fs_node_t *finddir_vfs(fs_node_t *node, const char *name);
int stat_vfs(struct fs_node *node, struct stat *buff);
s32int ioctl_vfs(fs_node_t *node, u32int, void *);
fs_node_t *get_path(const char *path);
fs_node_t *create_vfs(const char *path, u32int uid, u32int gid, u32int mode);
s32int register_fs(struct file_system_type *fs);
s32int mount(fs_node_t *dev, fs_node_t *dest, const char *fs_name, u32int flags);

#endif
