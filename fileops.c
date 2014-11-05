/* task.c - syscalls for file access/management */

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

#include <common.h>
#include <fileops.h>
#include <vfs.h>
#include <task.h>
#include <kmalloc.h>
#include <string.h>
#include <errno.h>

extern volatile task_t *current_task;

static int32_t valid_fd(int32_t fd) {
	if (fd < 0)
		return 0;
	if (fd > MAX_OF)
		return 0;
	if (!current_task->files[fd].file)
		return 0;

	return 1;
}

off_t lseek(int32_t fd, off_t off, int32_t whence) {
	if (!valid_fd(fd))
		return -EBADF;

	switch (whence) {
		case SEEK_SET:
			current_task->files[fd].off = off;
			break;
		case SEEK_CUR:
			current_task->files[fd].off += off;
			break;
		case SEEK_END:
			current_task->files[fd].off =
				current_task->files[fd].file->len + off;
			break;
		default:
			return -EINVAL;
	}

	return current_task->files[fd].off;
}

static int check_flags(fs_node_t *file, uint32_t flags) {
	int acceptable = 1;

	// Root is all powerful
	if (current_task->euid == 0)
		return acceptable;

	if (flags & O_RDONLY) {
		if (!(file->mode & VFS_O_READ)) {
			acceptable = 0;
			if (current_task->euid == file->uid && file->mode & VFS_U_READ)
				acceptable = 1;
			else if (current_task->egid == file->gid &&
					file->mode & VFS_G_READ)
				acceptable = 1;
		}
	}

	if (flags & O_WRONLY && acceptable == 1) {
		if (!(file->mode & VFS_O_WRITE)) {
			acceptable = 0;
			if (current_task->euid == file->uid && file->mode & VFS_U_WRITE)
				acceptable = 1;
			else if (current_task->egid == file->gid &&
					file->mode & VFS_G_WRITE)
				acceptable = 1;
		}
	}

	return acceptable;
}

ssize_t user_pread(int32_t fd, char *buf, size_t nbytes, off_t off) {
	if (!valid_fd(fd))
		return -EBADF;

	if (!buf)
		return -EFAULT;

	if (!(current_task->files[fd].file->flags & O_RDONLY))
		return -EINVAL;

	return read_vfs(current_task->files[fd].file, buf, nbytes, off);
}

ssize_t user_read(int32_t fd, char *buf, size_t nbytes) {
	if (!valid_fd(fd))
		return -EBADF;

	if (!buf)
		return -EFAULT;

	if (!(current_task->files[fd].file->flags & O_RDONLY))
		return -EINVAL;

	ssize_t ret = read_vfs(current_task->files[fd].file, buf, nbytes,
			current_task->files[fd].off);
	current_task->files[fd].off += ret;
	return ret;
}

ssize_t user_pwrite(int32_t fd, const char *buf, size_t nbytes, off_t off) {
	if (!valid_fd(fd))
		return -EBADF;

	if (!buf)
		return -EFAULT;

	if (!(current_task->files[fd].file->flags & O_WRONLY))
		return -EINVAL;

	return write_vfs(current_task->files[fd].file, buf, nbytes, off);
}

ssize_t user_write(int32_t fd, const char *buf, size_t nbytes) {
	if (!valid_fd(fd))
		return -EBADF;

	if (!buf)
		return -EFAULT;

	if (!(current_task->files[fd].file->flags & O_WRONLY))
		return -EINVAL;

	uint32_t ret = write_vfs(current_task->files[fd].file, buf, nbytes,
			current_task->files[fd].off);
	current_task->files[fd].off += ret;
	return ret;
}

static fs_node_t *getparent(const char *path, char *child) {
	if (!path)
		return NULL;

	char *path_cpy = (char *)kmalloc(strlen(path) + 1);
	if (!path_cpy)
		return NULL;
	strcpy(path_cpy, path);

	fs_node_t *parent;
	char *fname;

	for (fname = path_cpy + strlen(path_cpy); fname > path_cpy; fname--) {
		if (*fname == PATH_DELIMITER) {
			*fname = '\0';
			break;
		}
	}

	int32_t ret;
	if (fname == path_cpy) {
		char *name = ".";
		parent = kopen(name, O_WRONLY, &ret);
	} else 
		parent = kopen(path, O_WRONLY, &ret);

	fname++;

	if (strlen(fname) > NAME_MAX) {
		kfree(path_cpy);
		return NULL;
	}

	if (child) {
		strcpy(child, fname);
	}

	kfree(path_cpy);
	return parent;
}

int32_t user_open(const char *path, uint32_t flags, uint32_t mode) {
	if (!path)
		return -EFAULT;

	int i;
	for (i = 0; i < MAX_OF; i++)
		if (current_task->files[i].file == NULL)
			break;

	if (i == MAX_OF)
		return -ENFILE;

	int32_t ret;

	if (flags & O_CREAT) {
		char fname[NAME_MAX];
		fs_node_t *parent = getparent(path, fname);
		if (!parent)
			return -ENOENT;
		if (!check_flags(parent, O_WRONLY)) {
			close_vfs(parent);
			return -EACCES;
		}

		ret = create_vfs(parent, fname, current_task->euid, current_task->egid,
			mode | VFS_FILE, 0);
		close_vfs(parent);

		if (ret == -EEXIST && O_EXCL)
			return -EEXIST;
		else if (ret < 0 && ret != -EEXIST)
			return ret;
	}

	fs_node_t *file = kopen(path, flags, &ret);
	if (file && check_flags(file, flags)) {
		current_task->files[i].file = file;
		return i;
	} else if (file) {
		close_vfs(file);
		ret = -EINVAL;
	}

	return ret;
}

int32_t user_close(int32_t fd) {
	if (!valid_fd(fd))
		return -EBADF;

	uint32_t ret = close_vfs(current_task->files[fd].file);
	if (ret == 0)
		current_task->files[fd].file = NULL;
	return ret;
}

int32_t user_readdir(int32_t fd, struct dirent *dirp, uint32_t index) {
	if (!valid_fd(fd))
		return -EBADF;

	if (!dirp)
		return -EFAULT;

	return readdir_vfs(current_task->files[fd].file, dirp, index);
}

int32_t user_fstat(int32_t fd, struct stat *buff) {
	if (!valid_fd(fd))
		return -EBADF;

	if (!buff)
		return -EFAULT;

	return stat_vfs(current_task->files[fd].file, buff);
}

int32_t user_chmod(int32_t fd, uint32_t mode) {
	if (!valid_fd(fd))
		return -EBADF;

	if (current_task->euid == current_task->files[fd].file->uid ||
			current_task->euid == 0)
		return chmod_vfs(current_task->files[fd].file, mode);

	return -EPERM;
}

int32_t user_chown(int32_t fd, int32_t uid, int32_t gid) {
	if (!valid_fd(fd))
		return -EBADF;
	
	if (current_task->euid == current_task->files[fd].file->uid ||
			current_task->euid == 0)
		return chown_vfs(current_task->files[fd].file, uid, gid);

	return -EPERM;
}

int32_t user_ioctl(int32_t fd, uint32_t request, void *ptr) {
	if (!valid_fd(fd))
		return -EBADF;

	return ioctl_vfs(current_task->files[fd].file, request, ptr);
}

int32_t user_unlink(const char *path) {
	if (!path)
		return -EFAULT;

	char fname[NAME_MAX];
	fs_node_t *parent = getparent(path, fname);
	if (!parent)
		return -ENOENT;
	if (!check_flags(parent, O_WRONLY)) {
		close_vfs(parent);
		return -EACCES;
	}

	int32_t ret = unlink_vfs(parent, fname);
	close_vfs(parent);
	return ret;
}

int32_t mknod(const char *path, uint32_t mode, dev_t dev) {
	if (!path)
		return -EFAULT;

	char nname[NAME_MAX];
	fs_node_t *parent = getparent(path, nname);
	if (!parent)
		return -ENOENT;
	if (!check_flags(parent, O_WRONLY)) {
		close_vfs(parent);
		return -EACCES;
	}

	int32_t ret = create_vfs(parent, nname, current_task->euid,
		current_task->egid, mode, dev);
	close_vfs(parent);
	return ret;
}

int32_t user_mount(const char *src, const char *target, const char *fs_name,
		uint32_t flags) {
	if (current_task->euid != 0)
		return -EPERM;

	if (!target || !fs_name)
		return -EFAULT;

	int32_t ret;
	fs_node_t *src_node = kopen(src, flags, &ret);
	if (src && !src_node)
		return ret;

	ret = mount(src_node, target, fs_name, flags);

	if (ret != 0)
		close_vfs(src_node);

	return ret;
}