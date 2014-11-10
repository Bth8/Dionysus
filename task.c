/* task.c - functions for process creation and context switching. Also
 * switches stack from the minimal one set up in boot.s and provides several
 * user functions
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

#include <common.h>
#include <task.h>
#include <paging.h>
#include <string.h>
#include <kmalloc.h>
#include <gdt.h>
#include <vfs.h>
#include <errno.h>

// Defined in main.c
extern uint32_t initial_esp;

// Defined in process.s
extern uint32_t read_eip(void);

// Defined in paging.c
extern page_directory_t *current_dir;
extern page_directory_t *kernel_dir;

volatile task_t *current_task = NULL;
volatile task_t *ready_queue;
uint32_t pid_counter = 1;
uint8_t pid_lock = 0;

static uint32_t nextpid(void) {
	spin_lock(&pid_lock);
	task_t *iter = ready_queue;
	while (iter) {
		if (iter->id == pid_counter) {
			pid_counter++;
			iter = ready_queue;
			continue;
		}
		iter = iter->next;
	}
	spin_unlock(&pid_lock);
	return pid_counter++;
}

static void move_stack(void *new_stack_start, void *old_stack_start, size_t size) {
	uintptr_t i;
	// Allocate space
	for (i = (uintptr_t)new_stack_start; i >= (uintptr_t)new_stack_start - size;
			i -= 0x1000)
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
	for (i = (uint32_t)new_stack_start; i > (uint32_t)new_stack_start - size;
			i -= 4) {
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

void init_tasking(uintptr_t ebp) {
	asm volatile("cli");
	int i;
	// Relocate stack
	move_stack((void *)KERNEL_STACK_TOP, (void *)ebp, KERNEL_STACK_SIZE);

	// Init first task (kernel task)
	current_task = ready_queue = (task_t *)kmalloc(sizeof(task_t));
	ASSERT(current_task);

	current_task->id = nextpid();
	current_task->esp = current_task->ebp = 0;
	current_task->eip = 0;
	current_task->page_dir = current_dir;
	current_task->brk = 0;
	current_task->brk_actual = 0;
	current_task->start = 0;
	current_task->nice = 0;
	current_task->euid = current_task->suid = current_task->ruid = 0;
	current_task->egid = current_task->rgid = current_task->sgid = 0;
	current_task->next = NULL;
	current_task->cwd = kmalloc(2);
	ASSERT(current_task->cwd);
	current_task->cwd[0] = PATH_DELIMITER;
	current_task->cwd[1] = '\0';

	// Create a user stack
	for (i = USER_STACK_BOTTOM; i < USER_STACK_TOP; i += PAGE_SIZE);
		alloc_frame(get_page(i, 1, current_dir), 0, 1);

	// No open files yet
	for (i = 0; i < MAX_OF; i++)
		current_task->files[i].file = NULL;

	asm volatile("sti");
}

int32_t fork(void) {
	asm volatile("cli");
	int i;
	page_directory_t *directory = clone_directory(current_dir);

	// Create a new process
	task_t *new_task = (task_t *)kmalloc(sizeof(task_t));
	if (!new_task) {
		free_dir(directory);
		return -ENOMEM;
	}

	new_task->id = nextpid();
	new_task->esp = new_task->ebp = 0;
	new_task->eip = 0;
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
	new_task->next = NULL;
	new_task->cwd = (char *)kmalloc(strlen(current_task->cwd) + 1);
	if (!new_task->cwd) {
		free_dir(directory);
		kfree(new_task);
		return -ENOMEM;
	}
	strcpy(new_task->cwd, current_task->cwd);

	// Copy open files
	for (i = 0; i < MAX_OF; i++) {
		if (current_task->files[i].file != NULL) {
			new_task->files[i].file = clone_file(current_task->files[i].file);
			new_task->files[i].off = current_task->files[i].off;
		} else
			new_task->files[i].file = NULL;
	}

	// Add to end of ready queue
	task_t *task_i = (task_t *)ready_queue;
	while (task_i->next)
		task_i = task_i->next;
	task_i->next = new_task;
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
		return new_task->id;
	} else {
		// Send EOI to PIC. Otherwise, PIT won't fire again.
		outb(PIC_MASTER_A, PIC_COMMAND_EOI);
		return 0;
	}
}

int32_t getpid(void) {
	return current_task->id;
}

int switch_task(void) {
	if (current_task) {
		asm volatile("cli");
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

		current_task = current_task->next;
		// Make sure we didn't fall off the end
		if (!current_task)
			current_task = ready_queue;

		esp = current_task->esp;
		ebp = current_task->ebp;
		eip = current_task->eip;

		current_dir = current_task->page_dir;
		set_kernel_stack(esp);

		// Put new eip in ecx, load stack/base pointers, change page dir, put
		// dummy value in eax, jump to [ecx]
		asm volatile("mov %0, %%ecx; \
				mov %1, %%esp; \
				mov %2, %%ebp; \
				mov %3, %%cr3; \
				mov $0x12345, %%eax; \
				jmp *%%ecx" : : "r"(eip), "r"(esp), "r"(ebp),
				"r"(current_dir->physical_address) : 
				"ecx", "esp", "ebp", "eax");
	}

	return 0;
}

void exit_task(void) {
	asm volatile("cli");
	int i;
	task_t *iter = ready_queue
	task_t *current_cache = current_task;

	// Make sure we're not the only task
	ASSERT(iter->next != NULL);

	// Not the first task in the run queue
	if (iter != current_task) {
		// Find the previous task
		while (iter->next != current_task)
			iter = iter->next;
		// Point it to the next one. We're out of the run queue
		iter->next = current_task->next;
	} else {
		// Move the run queue to its second task
		iter = iter->next;
		ready_queue = iter;
	}
	// Prepare for a context switch
	current_task = iter;
	current_dir = current_task->page_dir;
	set_kernel_stack(current_task->esp);
	// Use new current_task's paging dir, because we're about to trash ours
	asm volatile("mov %0, %%cr3" : : "r"(current_dir->physical_address));

	// Free everything
	for (i = 0; i < MAX_OF; i++)
		if (current_cache->files[i].file)
			close_vfs(current_cache->files[i].file);

	free_dir(current_cache->page_dir);
	kfree(current_cache->cwd);
	kfree(current_cache);

	// Context switching time
	asm volatile("mov %0, %%ecx; \
			mov %1, %%esp; \
			mov %2, %%ebp; \
			mov $0x12345, %%eax; \
			sti; \
			jmp *%%ecx" ::
			"r"(current_task->eip),"r"(current_task->esp),
			"r"(current_task->ebp) :
			"ecx", "esp", "ebp", "eax");
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
			"r"(entry), "r"(argc), "r"(argv), "r"(envp), "r"(stack) :
			"esp", "eax");
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

int32_t setresuid(int32_t new_ruid, int32_t new_euid, int32_t new_suid) {
	int32_t set_ruid = current_task->ruid;
	int32_t set_euid = current_task->euid;
	int32_t set_suid = current_task->suid;

	if (set_euid != 0) {
		if (new_ruid > -1) {
			if (new_ruid == set_ruid ||
					new_ruid == set_euid ||
					new_ruid == set_suid)
				set_ruid = new_ruid;
			else
				return -EPERM;
		}
		if (new_euid > -1) {
			if (new_euid == set_ruid ||
					new_euid == set_euid ||
					new_euid == set_suid)
				set_euid = new_euid;
			else
				return -EPERM;
		}
		if (new_suid > -1) {
			if (new_suid == set_ruid ||
					new_suid == set_euid ||
					new_suid == set_suid)
				set_suid = new_suid;
			else
				return -EPERM;
		}
	} else {
		if (new_ruid > -1)
			set_ruid = new_ruid;
		if (new_euid > -1)
			set_euid = new_euid;
		if (new_suid > -1)
			set_suid = new_suid;
	}

	current_task->ruid = set_ruid;
	current_task->euid = set_euid;
	current_task->suid = set_suid;

	return 0;
}

int32_t getresuid(int32_t *ruid, int32_t *euid, int32_t *suid) {
	*ruid = current_task->ruid;
	*euid = current_task->euid;
	*suid = current_task->suid;
	return 0;
}

int32_t setresgid(int32_t new_rgid, int32_t new_egid, int32_t new_sgid) {
	int32_t set_rgid = current_task->rgid;
	int32_t set_egid = current_task->egid;
	int32_t set_sgid = current_task->sgid;

	if (set_egid != 0) {
		if (new_rgid > -1) {
			if (new_rgid == set_rgid ||
					new_rgid == set_egid ||
					new_rgid == set_sgid)
				set_rgid = new_rgid;
			else
				return -EPERM;
		}
		if (new_egid > -1) {
			if (new_egid == set_rgid ||
					new_egid == set_egid ||
					new_egid == set_sgid)
				set_egid = new_egid;
			else
				return -EPERM;
		}
		if (new_sgid > -1) {
			if (new_sgid == set_rgid ||
					new_sgid == set_egid ||
					new_sgid == set_sgid)
				set_sgid = new_sgid;
			else
				return -EPERM;
		}
	} else {
		if (new_rgid > -1)
			set_rgid = new_rgid;
		if (new_egid > -1)
			set_egid = new_egid;
		if (new_sgid > -1)
			set_sgid = new_sgid;
	}

	current_task->rgid = set_rgid;
	current_task->egid = set_egid;
	current_task->sgid = set_sgid;

	return 0;
}

int32_t getresgid(int32_t *rgid, int32_t *egid, int32_t *sgid) {
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