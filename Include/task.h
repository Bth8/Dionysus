/* task.h - task data types and declarations for initialization, creation and management */
/* Copyright (C) 2011, 2012 Bth8 <bth8fwd@gmail.com>
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

struct filep {
	fs_node_t *file;
	u32int off;
};

typedef struct task {
	u32int id;					// PID
	u32int esp, ebp;			// Stack and base pointers
	u32int eip;					// Instruction pointer
	page_directory_t *page_dir;
	u32int kernel_stack;
	s8int nice;
	int ruid, euid, suid;
	int rgid, egid, sgid;
	struct filep files[MAX_OF];
	struct task *next;			// Next task in linked list
} task_t;

void init_tasking(void);
int switch_task(void);
void exit_task(void);
int fork(void);
int nice(int inc);
void move_stack(void *new_stack_start, u32int size);
int getpid(void);
void switch_user_mode(void);
int setuid(int uid);
int seteuid(int new_euid);
int setreuid(int new_ruid, int new_euid);
int setresuid(int new_ruid, int new_euid, int new_suid);
int getuid(void);
int geteuid(void);
int getresuid(int *ruid, int *euid, int *suid);
int setgid(int gid);
int setegid(int new_egid);
int setregid(int new_rgid, int new_egid);
int setresgid(int new_rgid, int new_egid, int new_sgid);
int getgid(void);
int getegid(void);
int getresgid(int *ruid, int *euid, int *suid);
int lseek(int fd, u32int off, int whence);
int user_pread(int fd, char *buf, u32int nbytes, u32int off);
int user_read(int fd, char *buf, u32int nbytes);
int user_pwrite(int fd, const char *buf, u32int nbytes, u32int off);
int user_write(int fd, const char *buf, u32int nbytes);
int user_open(const char *path, u32int flags);
int user_close(int fd);
int user_ioctl(int fd, u32int request, void *ptr);

#endif
