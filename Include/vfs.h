/* vfs.h - basic VFS functions */

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
	uint32_t d_ino;
	char d_name[NAME_MAX];
};

struct file_ops {
	ssize_t(*read)(struct fs_node*, void*, size_t, off_t);
	ssize_t(*write)(struct fs_node*, const void*, size_t, off_t);
	int(*open)(struct fs_node*, uint32_t);
	int(*close)(struct fs_node*);
	struct fs_node*(*create)(struct fs_node*, const char*, uint32_t, uint32_t,
			uint32_t);
	int(*readdir)(struct fs_node*, struct dirent*, uint32_t);
	struct fs_node*(*finddir)(struct fs_node*, const char*);
	int (*mkdir)(struct fs_node*, char*, uint32_t);
	int (*stat)(struct fs_node*, struct stat*);
	int (*unlink)(struct fs_node*);
	int32_t (*ioctl)(struct fs_node*, uint32_t, void*);
};

typedef struct fs_node {
	char name[NAME_MAX];
	uint32_t mask;				// Permissions mask
	int gid;
	int uid;
	uint32_t flags;				// Includes node type
	uint32_t inode;				// Way for individual FSs to differentiate
								// between files
	size_t len;
	uint32_t impl;				// Implementation-defined
	struct file_ops ops;
	struct superblock *fs_sb;	// Parent fs
	struct superblock *ptr_sb;	// For mount points
	void *private_data;
} fs_node_t;

struct superblock {
	fs_node_t *dev;
	void *private_data;
	fs_node_t *root;
	uint32_t flags;
};

struct file_system_type {
	const char *name;
	uint32_t flags;
	struct superblock *(*get_super)(uint32_t, fs_node_t*);
	struct file_system_type *next;
};

// Spares are such that it matches newlib's definition
struct stat {
	uint32_t st_dev;
	uint32_t st_ino;
	uint32_t st_mode;
	uint16_t st_nlink;
	uint32_t st_uid;
	uint32_t st_gid;
	uint32_t st_rdev;
	size_t st_size;
	time_t st_atime;
	long st_spare1;
	time_t st_mtime;
	long st_spare2;
	time_t st_ctime;
	long st_spare3;
	uint32_t st_blksize;
	uint32_t st_blocks;
	long st_spare4[2];
};

extern fs_node_t *vfs_root;

uint32_t read_vfs(fs_node_t *node, void *buf, size_t count, off_t off);
uint32_t write_vfs(fs_node_t *node, const void *buf, size_t count, off_t off);
int open_vfs(fs_node_t *node, uint32_t flags);
int close_vfs(fs_node_t *node);
int readdir_vfs(fs_node_t *node, struct dirent *dirp, uint32_t index);
fs_node_t *finddir_vfs(fs_node_t *node, const char *name);
int stat_vfs(struct fs_node *node, struct stat *buff);
int32_t ioctl_vfs(fs_node_t *node, uint32_t, void *);
fs_node_t *get_path(const char *path);
fs_node_t *create_vfs(const char *path, uint32_t uid, uint32_t gid,
		uint32_t mode);
int unlink_vfs(struct fs_node *node);
int32_t register_fs(struct file_system_type *fs);
int32_t mount(fs_node_t *dev, fs_node_t *dest, const char *fs_name,
		uint32_t flags);

#endif /* VFS_H */
