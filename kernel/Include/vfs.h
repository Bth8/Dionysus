/* vfs.h - basic VFS functions */

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

#ifndef VFS_H
#define VFS_H
#include <common.h>
#include <time.h>

#define NAME_MAX		256
#define MAX_MNT_PTS		32

#define VFS_FILE		0x010000
#define VFS_DIR			0x020000
#define VFS_CHARDEV		0x040000
#define VFS_BLOCKDEV	0x080000
#define VFS_PIPE		0x100000
#define VFS_MOUNT		0x200000
#define VFS_LINK		0x400000
#define VFS_UNKNOWN		0x800000
#define VFS_TYPE_MASK	0xFF0000

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
#define VFS_PERM_MASK	07777

#define FS_NODEV		0x01

#define MNT_RDONLY		0x01
#define MNT_FORCE		0x02
#define MNT_DETACH		0x04

#define PATH_DELIMITER	'/'
#define PATH_THIS_DIR	"."
#define PATH_PARENT_DIR	".."

#define EOF				-1

#define MAJOR(x) ((x >> 24) & 0xFF)
#define MINOR(x) (x & 0xFFFFFF)
#define MKDEV(x, y) (((x & 0xFF) << 24) | (y & 0xFFFFFF))

struct fs_node;
struct stat;
struct dirent {
	ino_t d_ino;
	char d_name[NAME_MAX];
};

struct file_ops {
	ssize_t (*read)(struct fs_node*, void*, size_t, off_t);
	ssize_t (*write)(struct fs_node*, const void*, size_t, off_t);
	int32_t (*open)(struct fs_node*, uint32_t);
	int32_t (*close)(struct fs_node*);
	int32_t (*readdir)(struct fs_node*, struct dirent*, uint32_t);
	struct fs_node*(*finddir)(struct fs_node*, const char*);
	int32_t (*chmod)(struct fs_node*, mode_t);
	int32_t (*chown)(struct fs_node*, uid_t, gid_t);
	int32_t (*ioctl)(struct fs_node*, uint32_t, void*);
	int32_t (*create)(struct fs_node*, const char*, uid_t, gid_t,
		mode_t, dev_t);
	int32_t (*link)(struct fs_node *, struct fs_node *, const char*);
	int32_t (*unlink)(struct fs_node*, const char*);
};

typedef struct fs_node {
	ino_t inode;
	uid_t uid;
	gid_t gid;
	mode_t mode;				// Permissions mask and node type
	uint32_t flags;				// Flags used to open file. Tells us
								// what restrictions we've put on ourselves
	size_t len;
	dev_t dev;					// Implementation-defined

	time_t atime;
	time_t mtime;
	time_t ctime;

	struct file_ops ops;
	struct superblock *fs_sb;	// Parent fs
	void *private_data;
	int32_t refcount;
	nlink_t nlink;
} fs_node_t;

struct superblock {
	dev_t dev;
	void *private_data;
	fs_node_t *root;
	size_t blocksize;
	uint32_t flags;
	int32_t (*close_fs)(struct superblock*, uint32_t);
};

struct mountpoint {
	char *name;
	struct superblock *sb;
};

typedef struct file_system_type {
	uint32_t flags;
	struct superblock *(*get_super)(dev_t, uint32_t);
} file_system_t;

struct stat {
	dev_t st_dev;
	ino_t st_ino;
	mode_t st_mode;
	nlink_t st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	dev_t st_rdev;
	size_t st_size;
	time_t st_atime;
	time_t st_mtime;
	time_t st_ctime;
	size_t st_blksize;
	uint32_t st_blocks;
};

extern fs_node_t *vfs_root;

void init_vfs(void);
char *canonicalize_path(const char *cwd, const char *relpath);
ssize_t read_vfs(fs_node_t *node, void *buf, size_t count, off_t off);
ssize_t write_vfs(fs_node_t *node, const void *buf, size_t count, off_t off);
int32_t open_vfs(fs_node_t *node, uint32_t flags);
int32_t close_vfs(fs_node_t *node);
int32_t readdir_vfs(fs_node_t *node, struct dirent *dirp, uint32_t index);
int32_t stat_vfs(struct fs_node *node, struct stat *buff);
int32_t chmod_vfs(fs_node_t *node, mode_t mode);
int32_t chown_vfs(fs_node_t *node, uid_t uid, gid_t gid);
int32_t ioctl_vfs(fs_node_t *node, uint32_t req, void *data);
int32_t create_vfs(fs_node_t *parent, const char *fname, uid_t uid, 
		gid_t gid, mode_t mode, dev_t dev);
int32_t link_vfs(fs_node_t *parent, fs_node_t *child, const char *fname);
int32_t unlink_vfs(fs_node_t *parent, const char *fname);
fs_node_t *kopen(const char *relpath, int32_t flags, int32_t *openret);
int32_t register_fs(const char *name, struct file_system_type *fs);
int32_t mount(fs_node_t *dev, const char *path, const char *fs_name,
		uint32_t flags);
int32_t umount(const char *relpath, uint32_t flags);
fs_node_t *clone_file(fs_node_t *node);

#endif /* VFS_H */
