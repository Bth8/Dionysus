/* task.c - functions for process creation and context switching. Also
 * switches stack from the minimal one set up in boot.s and provides several
 * user functions
 */

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

/* Process organization is similar to linux. PID 1 (init) owns everything. Its
 * children are session leaders.
 */

#include <common.h>
#include <task.h>
#include <paging.h>
#include <string.h>
#include <kmalloc.h>
#include <idt.h>
#include <gdt.h>
#include <vfs.h>
#include <errno.h>
#include <structures/tree.h>
#include <structures/list.h>

#define PUSH(esp, type, object) ({ \
	esp -= sizeof(type); \
	*((type *)esp) = (type)object; })

// Defined in main.c
extern uint32_t initial_esp;

// Defined in process.s
extern uint32_t read_eip(void);

// Defined in paging.c
extern page_directory_t *current_dir;
extern page_directory_t *kernel_dir;

task_t *kidle = NULL;
volatile task_t *current_task = NULL;
list_t *processes = NULL;
list_t *run_queue = NULL;
tree_t *proc_tree = NULL;

// We get the first free pid, rather than go in order
static pid_t nextpid(void) {
	pid_t pid = 1;
	while (1) {
		if (pid > MAX_PID)
			return 0;
		node_t *node;
		foreach(node, processes) {
			if (((task_t *)node->data)->pid == pid)
				break;
		}
		if (node) {
			pid++;
			continue;
		}
		break;
	}

	return pid;
}

static task_t *get_task(pid_t pid) {
	if (pid == 0 || pid > MAX_PID)
		return NULL;

	node_t *process;
	foreach(process, processes) {
		if (((task_t *)process->data)->pid == pid)
			break;
	}

	if (!process)
		return NULL;
	return (task_t *)process->data;
}

static task_t *get_ready_task() {
	// It would be really bad to run out of memory right here...
	node_t *queue_node;
	queue_node = run_queue->head;
	task_t *task = NULL;
	if (queue_node) {
		list_dequeue(run_queue, queue_node);
		task = (task_t *)queue_node->data;
		kfree(queue_node);
	} else
		task = kidle;
	return task;
}

static void context_switch(void) {
	asm volatile("mov %0, %%ecx; \
		mov %1, %%esp; \
		mov %2, %%ebp; \
		mov %3, %%cr3; \
		mov $0x12345, %%eax; \
		jmp *%%ecx" : : "r"(current_task->eip), "r"(current_task->esp),
		"r"(current_task->ebp), "r"(current_dir->physical_address) : "ecx");
}

static void move_stack(void *new_stack_start, void *old_stack_start, size_t size) {
	uintptr_t i;
	// Allocate space
	for (i = (uintptr_t)new_stack_start - sizeof(uintptr_t);
			i >= (uintptr_t)new_stack_start - size; i -= PAGE_SIZE)
		alloc_frame(get_page(i, 1, current_dir), 1, 1);

	uintptr_t old_esp;
	uintptr_t old_ebp;

	asm volatile("mov %%esp, %0" : "=r" (old_esp));
	asm volatile("mov %%ebp, %0" : "=r" (old_ebp));

	off_t offset = (uintptr_t)(new_stack_start - old_stack_start);
	uint32_t new_esp = old_esp + offset;
	uint32_t new_ebp = old_ebp + offset;

	// Copy old stack contents into new one
	memcpy((void *)new_esp, (void *)old_esp, (uintptr_t)old_stack_start - old_esp);

	// Fix pointers (and hopefully not much else)
	for (i = (uint32_t)new_stack_start - sizeof(uintptr_t);
			i > (uint32_t)new_stack_start - size; i -= sizeof(uintptr_t)) {
		uintptr_t tmp = *(uintptr_t *)i;
		if (old_esp < tmp && tmp < (uintptr_t)old_stack_start) {
			tmp += offset;
			uint32_t *tmp2 = (uint32_t *)i;
			*tmp2 = tmp;
		}
	}

	// Actually change stack
	asm volatile("mov %0, %%esp" :: "r" (new_esp));
	asm volatile("mov %0, %%ebp" :: "r" (new_ebp));
}

static void _kidle(void) {
	halt();
}

void init_tasking(uintptr_t ebp) {
	asm volatile("cli");
	int i;
	// Relocate stack
	move_stack((void *)KERNEL_STACK_TOP, (void *)ebp, KERNEL_STACK_SIZE);

	run_queue = list_create();
	processes = list_create();
	proc_tree = tree_create();
	ASSERT(run_queue);
	ASSERT(processes);
	ASSERT(proc_tree);

	// Create init
	task_t *init = (task_t *)kmalloc(sizeof(task_t));
	ASSERT(init);

	memset(init, 0, sizeof(task_t));

	init->pid = nextpid();
	ASSERT(init->pid != 0);
	init->gid = init->pid;
	init->sid = init->pid;
	init->page_dir = current_dir;
	init->nice = 0;
	init->euid = init->suid = init->ruid = 0;
	init->egid = init->rgid = init->sgid = 0;
	init->cwd = kmalloc(2);
	ASSERT(init->cwd);
	init->cwd[0] = PATH_DELIMITER;
	init->cwd[1] = '\0';
	init->cmd = kmalloc(7);
	strcpy(init->cmd, "[init]");

	// No open files yet
	for (i = 0; i < MAX_OF; i++)
		init->files[i].file = NULL;

	// Create kidle
	tasklet_t *kidle_tasklet = (tasklet_t *)kmalloc(sizeof(tasklet_t));
	ASSERT(kidle_tasklet);

	kidle = &kidle_tasklet->task;

	memset(kidle_tasklet, 0, sizeof(task_t));

	kidle_tasklet->task.pid = -1;
	kidle_tasklet->task.page_dir = kernel_dir;
	kidle_tasklet->task.cmd = kmalloc(8);
	ASSERT(kidle_tasklet->task.cmd);
	strcpy(kidle_tasklet->task.cmd, "[kidle]");
	kidle_tasklet->stack = kmalloc(KERNEL_STACK_SIZE);
	ASSERT(kidle_tasklet->stack);
	kidle_tasklet->task.esp = (uintptr_t)kidle_tasklet->stack + KERNEL_STACK_SIZE;
	kidle_tasklet->task.ebp = kidle_tasklet->task.esp;
	kidle_tasklet->task.eip = (uintptr_t)&_kidle;

	// Create a user stack
	for (i = USER_STACK_BOTTOM; i < USER_STACK_TOP; i += PAGE_SIZE)
		alloc_frame(get_page(i, 1, current_dir), 0, 1);

	tree_node_t *treenode = tree_set_root(proc_tree, init);
	ASSERT(treenode);
	init->treenode = treenode;
	ASSERT(list_insert(processes, init));
	current_task = init;

	asm volatile("sti");
}

pid_t fork(void) {
	asm volatile("cli");
	int i;
	page_directory_t *directory = clone_directory(current_dir);

	// Create a new process
	task_t *new_task = (task_t *)kmalloc(sizeof(task_t));
	if (!new_task) {
		free_dir(directory);
		return -ENOMEM;
	}

	memset(new_task, 0, sizeof(task_t));

	new_task->pid = nextpid();
	if (new_task->pid == 0) {
		free_dir(directory);
		kfree(new_task);
		asm volatile("sti");
		return -EAGAIN;
	}
	new_task->gid = current_task->gid;
	new_task->sid = current_task->sid;
	if (current_task->pid == 1)
		new_task->gid = new_task->sid = new_task->pid;
	else if (current_task->pid == current_task->sid)
		new_task->gid = new_task->pid;

	new_task->page_dir = directory;
	new_task->brk = current_task->brk;
	new_task->brk_actual = current_task->brk_actual;
	new_task->start = current_task->start;
	// Inherit niceness and ids of parent
	new_task->nice = current_task->nice;
	new_task->euid = current_task->euid;
	new_task->ruid = current_task->ruid;
	new_task->suid = current_task->suid;
	new_task->egid = current_task->egid;
	new_task->rgid = current_task->rgid;
	new_task->sgid = current_task->sgid;
	new_task->cwd = (char *)kmalloc(strlen(current_task->cwd) + 1);
	if (!new_task->cwd)
		goto error1;
	new_task->cmd = (char *)kmalloc(strlen(current_task->cmd) + 1);
	if (!new_task->cmd)
		goto error2;
	strcpy(new_task->cwd, current_task->cwd);
	strcpy(new_task->cmd, current_task->cmd);

	// Copy open files
	for (i = 0; i < MAX_OF; i++) {
		if (current_task->files[i].file != NULL) {
			new_task->files[i].file = clone_file(current_task->files[i].file);
			new_task->files[i].off = current_task->files[i].off;
		} else
			new_task->files[i].file = NULL;
	}

	tree_node_t *treenode = tree_insert_node(proc_tree, current_task->treenode,
		new_task);
	if (!treenode)
		goto error3;

	new_task->treenode = treenode;

	node_t *proc;
	node_t *proc_node = NULL;
	foreach(proc, processes) {
		if (((task_t *)proc->data)->pid > new_task->pid)
			break;
	}
	if (proc)
		proc_node = list_insert_before(processes, proc, new_task);
	else
		proc_node = list_insert(processes, new_task);
	if (!proc_node)
		goto error4;

	node_t *queue_node = list_insert(run_queue, new_task);
	if (!queue_node)
		goto error5;

	// Entry point for new process
	uint32_t esp;
	uint32_t ebp;
	asm volatile("mov %%esp, %0" : "=r" (esp));
	asm volatile("mov %%ebp, %0" : "=r" (ebp));
	uint32_t eip = read_eip();

	// Are we parent or child? See switch_task for explanation
	if (eip != 0x12345) {
		new_task->esp = esp;
		new_task->ebp = ebp;
		new_task->eip = eip;
		asm volatile("sti");
		return new_task->pid;
	} else
		return 0;

error5:
	list_dequeue(processes, proc_node);
	kfree(proc_node);
error4:
	tree_detach_branch(proc_tree, treenode);
	tree_delete_node(treenode);
error3:
	for (i = 0; i < MAX_OF; i++) {
		if (current_task->files[i].file != NULL) {
			close_vfs(new_task->files[i].file);
		}
	}
	kfree(new_task->cmd);
error2:
	kfree(new_task->cwd);
error1:
	free_dir(directory);
	kfree(new_task);
	asm volatile("sti");
	return -ENOMEM;
}

void _tasklet_finish(void) {
	asm volatile("cli");

	tasklet_t *tasklet = container_of((task_t *)current_task, tasklet_t, task);

	tasklet->scheduled = 0;

	switch_task(0);
}

tasklet_t *create_tasklet(tasklet_body_t body, const char *name, void *argp) {
	ASSERT(name);

	tasklet_t *tasklet = (tasklet_t *)kmalloc(sizeof(tasklet_t));
	if (!tasklet)
		return NULL;

	memset(tasklet, 0, sizeof(tasklet_t));

	asm volatile("cli");

	tasklet->task.pid = nextpid();
	if (tasklet->task.pid == 0) {
		kfree(tasklet);
		asm volatile("sti");
		return NULL;
	}

	tasklet->task.page_dir = kernel_dir;

	tasklet->task.cmd = kmalloc(strlen(name) + 1);
	if (!tasklet->task.cmd)
		goto error1;
	strcpy(tasklet->task.cmd, name);

	tasklet->stack = kmalloc(KERNEL_STACK_SIZE);
	if (!tasklet->stack)
		goto error2;

	tasklet->entry = (uintptr_t)body;
	tasklet->argp = argp;
	tasklet->scheduled = 0;

	tasklet->task.esp = (uintptr_t)tasklet->stack + KERNEL_STACK_SIZE;
	tasklet->task.ebp = tasklet->task.esp;
	tasklet->task.eip = tasklet->entry;

	PUSH(tasklet->task.esp, uintptr_t, tasklet->argp);
	PUSH(tasklet->task.esp, uintptr_t, &_tasklet_finish);

	tree_node_t *treenode = tree_insert_node(proc_tree, proc_tree->root,
		&tasklet->task);
	if (!treenode)
		goto error3;
	tasklet->task.treenode = treenode;

	node_t *proc;
	node_t *proc_node = NULL;
	foreach(proc, processes) {
		if (((task_t *)proc->data)->pid > tasklet->task.pid)
			break;
	}
	if (proc)
		proc_node = list_insert_before(processes, proc, &tasklet->task);
	else
		proc_node = list_insert(processes, &tasklet->task);
	if (!proc_node)
		goto error4;

	asm volatile("sti");
	return tasklet;

error4:
	tree_detach_branch(proc_tree, treenode);
	tree_delete_node(treenode);
error3:
	kfree(tasklet->stack);
error2:
	kfree(tasklet->task.cmd);
error1:
	kfree(tasklet);
	asm volatile("sti");
	return NULL;
}

int32_t schedule_tasklet(tasklet_t *tasklet) {
	asm volatile("cli");

	ASSERT(tasklet);

	if (tasklet->scheduled) {
		asm volatile("sti");
		return 0;
	}

	node_t *queue_node = list_insert(run_queue, &tasklet->task);

	if (!queue_node) {
		asm volatile("sti");
		return -ENOMEM;
	}

	tasklet->scheduled = 1;

	asm volatile("sti");

	return 0;
}

void reset_tasklet(tasklet_t *tasklet) {
	ASSERT(!tasklet->scheduled);

	tasklet->task.esp = (uintptr_t)tasklet->stack + KERNEL_STACK_SIZE;
	tasklet->task.ebp = tasklet->task.esp;
	tasklet->task.eip = tasklet->entry;

	PUSH(tasklet->task.esp, uintptr_t, tasklet->argp);
	PUSH(tasklet->task.esp, uintptr_t, &_tasklet_finish);
}

int32_t reset_and_reschedule(tasklet_t *tasklet) {
	if (tasklet->scheduled)
		return 0;

	reset_tasklet(tasklet);

	return schedule_tasklet(tasklet);
}

void destroy_tasklet(tasklet_t *tasklet) {
	if (!tasklet)
		return;

	asm volatile("cli");

	ASSERT(!tasklet->scheduled);

	tree_detach_branch(proc_tree, tasklet->task.treenode);
	tree_delete_node(tasklet->task.treenode);

	node_t *proc_node = list_find(processes, &tasklet->task);

	list_dequeue(processes, proc_node);
	kfree(proc_node);

	kfree(tasklet->task.cmd);
	kfree(tasklet->stack);
	kfree(tasklet);

	asm volatile("sti");
}

int switch_task(int reschedule) {
	if (current_task) {
		uint32_t esp, ebp, eip;
		asm volatile("mov %%esp, %0" : "=r" (esp));
		asm volatile("mov %%ebp, %0" : "=r" (ebp));

		/* Cunning logic time!
		 * We're going to be in one of 2 states after this call
		 * Either read_eip just returned, or we've just switched tasks and
		 * have come back here
		 * If it's the latter, we need to return. To detect it, the former
		 * will return 0x12345
		 */
		eip = read_eip();
		if (eip == 0x12345)
			return current_task->nice;

		current_task->eip = eip;
		current_task->esp = esp;
		current_task->ebp = ebp;

		// It would be really bad to run out of memory right here...
		node_t *queue_node;
		if (reschedule && current_task->pid != -1) {
			queue_node = list_insert(run_queue, (task_t *)current_task);
			ASSERT(queue_node);
		}

		current_task = get_ready_task();

		esp = current_task->esp;
		ebp = current_task->ebp;
		eip = current_task->eip;

		current_dir = current_task->page_dir;
		set_kernel_stack(esp);

		context_switch();
	}

	return 0;
}

void exit_task(int32_t status) {
	asm volatile("cli");
	ASSERT(current_task->pid != 1); // Init doesn't exit

	task_t *current_cache = (task_t *)current_task;

	current_cache->exit = status;

	// Switch tasks
	current_task = get_ready_task();

	// We don't delete from the process tree/list because we haven't been
	// waited on

	// Init inherits orphans
	tree_inherit_children(proc_tree, proc_tree->root, current_cache->treenode);

	// Use new current_task's paging dir, because we're about to trash ours
	current_dir = current_task->page_dir;
	set_kernel_stack(current_task->esp);
	asm volatile("mov %0, %%cr3" : : "r"(current_dir->physical_address));

	// Free everything
	int i;
	for (i = 0; i < MAX_OF; i++)
		if (current_cache->files[i].file)
			close_vfs(current_cache->files[i].file);

	free_dir(current_cache->page_dir);
	kfree(current_cache->cwd);

	context_switch();
}

waitqueue_t *create_waitqueue(void) {
	waitqueue_t *wq = (waitqueue_t *)kmalloc(sizeof(waitqueue_t));
	if (!wq)
		return NULL;
	wq->queue = list_create();
	if (!wq->queue) {
		kfree(wq);
		return NULL;
	}

	return wq;
}

// This doesn't really need to be atomic
void destroy_waitqueue(waitqueue_t *queue) {
	ASSERT(queue->queue->head == NULL);
	list_destroy(queue->queue);
	kfree(queue);
}

int sleep_thread(waitqueue_t *wq, uint32_t flags) {
	/* Normally, switch_task is called from an interrupt context,
	 * so all registers are saved automatically. Here, that's not the
	 * case, so we have to convince gcc to save everything for us
	 */
	asm volatile("" : : : "ebx", "esi", "edi");

	ASSERT(wq);
	asm volatile("cli");

	current_task->sleep_flags = SLEEP_ASLEEP | flags;
	current_task->wq = wq;
	node_t *queue_node = list_insert(wq->queue, (task_t *)current_task);
	ASSERT(queue_node);

	switch_task(0);

	return current_task->sleep_flags & SLEEP_INTERRUPTED;
}

void wake_queue(waitqueue_t *wq) {
	ASSERT(wq);

	asm volatile("cli");
	node_t *node = wq->queue->head;
	while (node) {
		node_t *cache = node;
		node = node->next;
		task_t *task = (task_t *)cache->data;
		list_dequeue(wq->queue, cache);
		kfree(cache);

		task->sleep_flags &= ~SLEEP_ASLEEP;
		task->wq = NULL;
		cache = list_insert(run_queue, task);
		ASSERT(cache);
	}
	asm volatile("sti");
}

void switch_user_mode(uint32_t entry, int32_t argc, char **argv, char **envp,
		uint32_t stack) {
	set_kernel_stack(current_task->esp);

	// First entry on stack will be 0. Protects from page fault.
	stack -= 4;

	asm volatile("cli; \
		mov %4, %%esp; \
		pushl %3; \
		pushl %2; \
		pushl %1; \
		mov $0x23, %%ax; \
		mov %%ax, %%ds; \
		mov %%ax, %%es; \
		mov %%ax, %%fs; \
		mov %%ax, %%gs; \
		mov %%esp, %%eax; \
		pushl $0x23; \
		pushl %%eax; \
		pushf; \
		pop %%eax; \
		or $0x200, %%eax; \
		pushl %%eax; \
		pushl $0x1B; \
		pushl %0; \
		iret;": :
		"r"(entry), "r"(argc), "r"(argv), "r"(envp), "r"(stack));
}

pid_t getpid(void) {
	return current_task->pid;
}

pid_t setpgid(pid_t pid, pid_t pgid) {
	if (pid < 0 || pgid < 0)
		return -EINVAL;

	asm volatile("cli");

	task_t *task = (task_t *)current_task;
	if (pid != 0) {
		task = get_task(pid);
		if (!task) {
			return -ESRCH;
		}

		tree_node_t *treenode;
		for (treenode = task->treenode; treenode; treenode = treenode->parent)
			if (treenode == current_task->treenode)
				break;

		if (!treenode) {
			return -ESRCH;
		}
	}

	if (task->pid == task->sid)
		return -EPERM;

	if (task->sid != current_task->sid)
		return -EPERM;

	if (pgid == 0 || pgid == task->pid) {
		pgid = task->pid;
		task->gid = task->pid;
		return pgid;
	}

	task_t *groupleader = get_task(pgid);
	if (!groupleader) {
		node_t *node;
		foreach(node, processes) {
			task_t *task = (task_t *)node->data;
			if (task->gid == pgid) {
				groupleader = task;
				break;
			}
		}
		if (!groupleader)
			return -ESRCH;
	}

	if (groupleader->gid != pgid)
		return -EPERM;

	if (groupleader->sid != task->sid)
		return -EPERM;

	task->gid = pgid;

	return pgid;
}

pid_t getpgid(pid_t pid) {
	if (pid < 0)
		return -EINVAL;
	if (pid == 0)
		return current_task->gid;

	task_t *task = get_task(pid);
	if (!task)
		return -ESRCH;

	return task->gid;
}

pid_t setsid(void) {
	if (current_task->gid == current_task->pid)
		return -EPERM;
	asm volatile("cli");

	tree_detach_branch(proc_tree, current_task->treenode);
	tree_insert_direct(proc_tree, proc_tree->root, current_task->treenode);

	current_task->sid = current_task->gid = current_task->pid;

	return current_task->sid;
}

pid_t getsid(void) {
	return current_task->sid;
}

int32_t nice(int32_t inc) {
	if (inc < 0 && current_task->euid != 0)
		return -1;
	if (current_task->nice + inc > 19)
		current_task->nice = 19;
	else if (current_task->nice + inc < -20)
		current_task->nice = -20;
	else
		current_task->nice += inc;
	return current_task->nice;
}

int32_t setresuid(uid_t new_ruid, uid_t new_euid, uid_t new_suid) {
	uid_t set_ruid = current_task->ruid;
	uid_t set_euid = current_task->euid;
	uid_t set_suid = current_task->suid;

	if (set_euid != 0) {
		if (new_ruid == set_ruid ||
				new_ruid == set_euid ||
				new_ruid == set_suid)
			set_ruid = new_ruid;
		else
			return -EPERM;

		if (new_euid == set_ruid ||
				new_euid == set_euid ||
				new_euid == set_suid)
			set_euid = new_euid;
		else
			return -EPERM;

		if (new_suid == set_ruid ||
				new_suid == set_euid ||
				new_suid == set_suid)
			set_suid = new_suid;
		else
			return -EPERM;
	} else {
		set_ruid = new_ruid;
		set_euid = new_euid;
		set_suid = new_suid;
	}

	current_task->ruid = set_ruid;
	current_task->euid = set_euid;
	current_task->suid = set_suid;

	return 0;
}

int32_t getresuid(uid_t *ruid, uid_t *euid, uid_t *suid) {
	*ruid = current_task->ruid;
	*euid = current_task->euid;
	*suid = current_task->suid;
	return 0;
}

int32_t setresgid(gid_t new_rgid, gid_t new_egid, gid_t new_sgid) {
	gid_t set_rgid = current_task->rgid;
	gid_t set_egid = current_task->egid;
	gid_t set_sgid = current_task->sgid;

	if (set_egid != 0) {
		if (new_rgid == set_rgid ||
				new_rgid == set_egid ||
				new_rgid == set_sgid)
			set_rgid = new_rgid;
		else
			return -EPERM;

		if (new_egid == set_rgid ||
				new_egid == set_egid ||
				new_egid == set_sgid)
			set_egid = new_egid;
		else
			return -EPERM;

		if (new_sgid == set_rgid ||
				new_sgid == set_egid ||
				new_sgid == set_sgid)
			set_sgid = new_sgid;
		else
			return -EPERM;
	} else {
		set_rgid = new_rgid;
		set_egid = new_egid;
		set_sgid = new_sgid;
	}

	current_task->rgid = set_rgid;
	current_task->egid = set_egid;
	current_task->sgid = set_sgid;

	return 0;
}

int32_t getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid) {
	*rgid = current_task->rgid;
	*egid = current_task->egid;
	*sgid = current_task->sgid;
	return 0;
}

uintptr_t sbrk(uintptr_t inc) {
	uintptr_t ret = current_task->brk;
	while (current_task->brk_actual < current_task->brk + inc) {
		current_task->brk_actual += 0x1000;
		alloc_frame(get_page(current_task->brk_actual, 1, current_dir), 0, 1);
	}

	current_task->brk += inc;

	return ret;
}

// Not much error checking, but if we're in a bad place, other
// permissions will save the day
uint32_t chdir(const char *path) {
	if (!path)
		return -EFAULT;

	char *new_cwd = canonicalize_path(current_task->cwd, path);
	kfree(current_task->cwd);
	current_task->cwd = new_cwd;

	return 0;
}