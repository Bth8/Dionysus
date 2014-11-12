/* fileops.h - functions for the creation, modification, access, and management 
 * of files
 */

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

#ifndef FILEOPS_H
#define FILEOPS_H

#include <common.h>
#include <vfs.h>

off_t lseek(int32_t fd, off_t off, int32_t whence);
ssize_t user_pread(int32_t fd, char *buf, size_t nbytes, off_t off);
ssize_t user_read(int32_t fd, char *buf, size_t nbytes);
ssize_t user_pwrite(int32_t fd, const char *buf, size_t nbytes, off_t off);
ssize_t user_write(int32_t fd, const char *buf, size_t nbytes);
int32_t user_open(const char *path, uint32_t flags, uint32_t mode);
int32_t user_close(int32_t fd);
int32_t user_readdir(int32_t fd, struct dirent *dirp, uint32_t index);
int32_t user_fstat(int32_t fd, struct stat *buff);
int32_t user_chmod(int32_t fd, uint32_t mode);
int32_t user_chown(int32_t fd, int32_t uid, int32_t gid);
int32_t user_ioctl(int32_t fd, uint32_t request, void *ptr);
int32_t user_link(const char *oldpath, const char *newpath);
int32_t user_unlink(const char *path);
int32_t mknod(const char *path, uint32_t mode, dev_t dev);
int32_t user_mount(const char *src, const char *target, const char *fs_name,
		uint32_t flags);
int32_t user_umount(const char *target, uint32_t flags);

#endif /* FILEOPS_H */