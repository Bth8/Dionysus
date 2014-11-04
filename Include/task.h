/* task.h - task data types and declarations for initialization, creation
 * and management */

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

#ifndef TASK_H
#define TASK_H

#include <common.h>
#include <paging.h>
#include <vfs.h>

#define KERNEL_STACK_SIZE 2048
#define MAX_OF 32

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define USER_STACK_BOTTOM 0x10000000
#define USER_STACK_TOP 0x10004000

struct filep {
	fs_node_t *file;
	off_t off;
};

typedef struct task {
	uint32_t id;				// PID
	uintptr_t esp, ebp;			// Stack and base pointers
	uintptr_t eip;				// Instruction pointer
	page_directory_t *page_dir;
	uintptr_t brk;				// Heap end
	uintptr_t brk_actual;		// Actual end of the memory allocated for
								// the heap
	uintptr_t start;			// Image start
	int8_t nice;
	int ruid, euid, suid;
	int rgid, egid, sgid;
	char *cwd;
	struct filep files[MAX_OF];
	struct task *next;			// Next task in linked list
} task_t;

void init_tasking(uintptr_t ebp);
int32_t fork(void);
int32_t getpid(void);
int switch_task(void);
void exit_task(void);
void switch_user_mode(uint32_t entry, int32_t argc, char **argv, char **envp,
		uint32_t stack);
int32_t nice(int32_t inc);
int32_t setresuid(int32_t new_ruid, int32_t new_euid, int32_t new_suid);
int32_t getresuid(int32_t *ruid, int32_t *euid, int32_t *suid);
int32_t setresgid(int32_t new_rgid, int32_t new_egid, int32_t new_sgid);
int32_t getresgid(int32_t *ruid, int32_t *euid, int32_t *suid);
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
int32_t user_unlink(const char *path);
int32_t user_mount(const char *src, const char *target, const char *fs_name,
		uint32_t flags);
uintptr_t sbrk(uintptr_t inc);

#endif /* TASK_H */
