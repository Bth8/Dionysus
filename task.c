/* task.c - C functions for process creation and context switching. Also switches stack from the minimal one set up in boot.s */
/* Copyright (C) 2011 Bth8 <bth8fwd@gmail.com>
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

// Defined in main.c
extern u32int initial_esp;

// Defined in process.s
extern u32int read_eip();
extern void fake_iret();

// Defined in paging.c
extern page_directory_t *current_dir;
extern page_directory_t *kernel_dir;

volatile task_t *current_task;
volatile task_t *ready_queue;
u32int next_pid = 1;

void move_stack(void *new_stack_start, u32int size) {
	u32int i;
	// Allocate space
	for (i = (u32int)new_stack_start; i >= (u32int)new_stack_start - size; i -= 0x1000)
		alloc_frame(get_page(i, 1, current_dir), 0, 1, 0);

	switch_page_dir(current_dir);	// Flush TLB

	u32int old_esp;
	u32int old_ebp;

	asm volatile("mov %%esp, %0" : "=r" (old_esp));
	asm volatile("mov %%ebp, %0" : "=r" (old_ebp));

	u32int offset = (u32int)new_stack_start - initial_esp;
	u32int new_esp = old_esp + offset;
	u32int new_ebp = old_ebp + offset;

	// Copy old stack contents into new one
	memcpy((u8int *)new_esp, (u8int *)old_esp, initial_esp - old_esp);

	// Fix ebps (and hopefully not much else)
	for (i = (u32int)new_stack_start; i > (u32int)new_stack_start - size; i -= 4) {
		u32int tmp = *(u32int *)i;
		if (old_esp < tmp && tmp < initial_esp) {
			tmp += offset;
			u32int *tmp2 = (u32int *)i;
			*tmp2 = tmp;
		}
	}

	// Actually change stack
	asm volatile("mov %0, %%esp" :: "r" (new_esp));
	asm volatile("mov %0, %%ebp" :: "r" (new_ebp));
}

void init_tasking() {
	asm volatile("cli");
	// Relocate stack
	move_stack((void *)0xF0000000, 0x2000);

	// Init first task (kernel task)
	current_task = ready_queue = (task_t *)kmalloc(sizeof(task_t));
	current_task->id = next_pid++;
	current_task->esp = current_task->ebp = 0;
	current_task->eip = 0;
	current_task->page_dir = current_dir;
	current_task->next = NULL;
	current_task->kernel_stack = (u32int)kmalloc_a(KERNEL_STACK_SIZE);

	asm volatile("sti");
}

int fork() {
	asm volatile("cli");
	page_directory_t *directory = clone_directory(current_dir);

	// Create a new process
	task_t *new_task = (task_t *)kmalloc(sizeof(task_t));
	new_task->id = next_pid++;
	new_task->esp = new_task->ebp = 0;
	new_task->eip = 0;
	new_task->page_dir = directory;
	current_task->kernel_stack = (u32int)kmalloc_a(KERNEL_STACK_SIZE);
	new_task->next = NULL;

	// Add to end of ready queue
	task_t *task_i = (task_t *)ready_queue;
	while (task_i->next)
		task_i = task_i->next;
	task_i->next = new_task;
	// Entry point for new process
	u32int esp;
	u32int ebp;
	asm volatile("mov %%esp, %0" : "=r" (esp));
	asm volatile("mov %%ebp, %0" : "=r" (ebp));
	u32int eip = read_eip();

	// Are we parent or child?
	if (eip != 0x12345) {
		new_task->esp = esp;
		new_task->ebp = ebp;
		new_task->eip = eip;
		asm volatile("sti");
		return new_task->id;
	} else {
		fake_iret();
		return 0;
	}
}

int getpid() {
	return current_task->id;
}

void switch_task() {
	if (current_task) {
		asm volatile("cli");
		u32int esp, ebp, eip;
		asm volatile("mov %%esp, %0" : "=r" (esp));
		asm volatile("mov %%ebp, %0" : "=r" (ebp));

		// Cunning logic time!
		// We're going to be in one of 2 states after this call
		// Either read_eip just returned, or we've just switched tasks and have come back here
		// If it's the latter, we need to return. To detect it, the former will return 0x12345
		eip = read_eip();
		if (eip == 0x12345)
			return;

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
		set_kernel_stack(current_task->kernel_stack + KERNEL_STACK_SIZE);

		// Put new eip in ecx, load stack/base pointers, change page dir, put dummy value int eax, restart interrupts, jump to [ecx]
		asm volatile("mov %0, %%ecx;\
					mov %1, %%esp;\
					mov %2, %%ebp;\
					mov %3, %%cr3;\
					mov $0x12345, %%eax;\
					sti;\
					jmp *%%ecx" : : "r"(eip), "r"(esp), "r"(ebp), "r"(current_dir->physical_address) : "ecx", "esp", "ebp", "eax");
	}
}

void exit_task() {
	asm volatile("cli");
	task_t *task_i = (task_t *)ready_queue, *current_cache = (task_t *)current_task;
	ASSERT(task_i->next != NULL);
	if (task_i != current_task) {
		while (task_i->next != current_task)
			task_i = task_i->next;
		task_i->next = current_task->next;
	} else {
		task_i = task_i->next;
		ready_queue = task_i;
	}
	current_task = task_i;
	current_dir = current_task->page_dir;
	asm volatile("mov %0, %%cr3":: "r"(current_dir->physical_address));
	switch_task();
	free_dir(current_cache->page_dir);
	kfree((void *)current_cache->kernel_stack);
	kfree((void *)current_cache);
	asm volatile("sti");
}	

void switch_user_mode() {
	set_kernel_stack(current_task->kernel_stack + KERNEL_STACK_SIZE);

	asm volatile("cli;\
				mov $0x23, %%ax;\
				mov %%ax, %%ds;\
				mov %%ax, %%es;\
				mov %%ax, %%fs;\
				mov %%ax, %%gs;\
				mov %%esp, %%eax;\
				pushl $0x23;\
				pushl %%eax;\
				pushf;\
				pop %%eax;\
				or $0x200, %%eax;\
				pushl %%eax;\
				pushl $0x1B;\
				push $1f;\
				iret;\
			1:"::);
}
