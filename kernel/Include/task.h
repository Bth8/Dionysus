/* task.h - task data types and permissions operations */

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

#ifndef TASK_H
#define TASK_H

#include <common.h>
#include <paging.h>
#include <vfs.h>
#include <structures/list.h>
#include <structures/tree.h>

#define KERNEL_STACK_TOP	0xF0000000
#define KERNEL_STACK_SIZE	0x2000
#define MAX_OF				32
#define MAX_PID				32768

#define SEEK_SET			0
#define SEEK_CUR			1
#define SEEK_END			2

#define USER_STACK_BOTTOM	0x10000000
#define USER_STACK_TOP		0x10004000

#define SLEEP_ASLEEP		0x01
#define SLEEP_INTERRUPTABLE	0x02
#define SLEEP_INTERRUPTED	0x04

typedef void (*tasklet_body_t)(void*);

struct filep {
	fs_node_t *file;
	off_t off;
};

typedef struct {
	list_t *queue;
} waitqueue_t;

typedef struct {
	pid_t pid;
	pid_t gid;
	pid_t sid;
	uintptr_t esp, ebp;			// Stack and base pointers
	uintptr_t eip;				// Instruction pointer
	page_directory_t *page_dir;
	uintptr_t brk;				// Heap end
	uintptr_t brk_actual;		// Actual end of the memory allocated for
								// the heap
	uintptr_t start;			// Image start
	int32_t exit;
	int8_t nice;
	uid_t ruid, euid, suid;
	gid_t rgid, egid, sgid;
	char *cwd;
	char *cmd;
	struct filep files[MAX_OF];
	tree_node_t *treenode;		// Where it is in the process tree
	waitqueue_t *wq;
	uint32_t sleep_flags;
} task_t;

typedef struct {
	task_t task;
	void *stack;
	void *argp;
	uintptr_t entry;
	uint32_t scheduled;
} tasklet_t;

void init_tasking(uintptr_t ebp);
pid_t fork(void);
tasklet_t *create_tasklet(tasklet_body_t body, const char *name, void *argp);
int32_t schedule_tasklet(tasklet_t *tasklet);
void reset_tasklet(tasklet_t *tasklet);
int32_t reset_and_reschedule(tasklet_t *tasklet);
void destroy_tasklet(tasklet_t *tasklet);
int switch_task(int reschedule);
void exit_task(int32_t status);
waitqueue_t *create_waitqueue(void);
void destroy_waitqueue(waitqueue_t *queue);
int sleep_thread(waitqueue_t *wq, uint32_t flags);
void wake_queue(waitqueue_t *wq);
void switch_user_mode(uint32_t entry, int32_t argc, char **argv, char **envp,
		uint32_t stack);
pid_t getpid(void);
pid_t setpgid(pid_t pid, pid_t pgid);
pid_t getpgid(pid_t pid);
pid_t setsid(void);
pid_t getsid(void);
int32_t nice(int32_t inc);
int32_t setresuid(uid_t new_ruid, uid_t new_euid, uid_t new_suid);
int32_t getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
int32_t setresgid(gid_t new_rgid, gid_t new_egid, gid_t new_sgid);
int32_t getresgid(gid_t *ruid, gid_t *euid, gid_t *suid);
uintptr_t sbrk(uintptr_t inc);
uint32_t chdir(const char *path);

#define wait_event(wq, expr) do { if (expr) { break; } } \
	while (sleep_thread(wq, 0) == 0);
#define wait_event_interruptable(wq, expr) do { if (expr) { break; } } \
	while (sleep_thread(wq, SLEEP_INTERRUPTABLE) == 0);

#endif /* TASK_H */
