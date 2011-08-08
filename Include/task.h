/* task.h - task data types and declarations for initialization, creation and management */
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

#ifndef TASK_H
#define TASK_H
#include <common.h>
#include <paging.h>

#define KERNEL_STACK_SIZE 2048

typedef struct task {
	u32int id;					// PID
	u32int esp, ebp;			// Stack and base pointers
	u32int eip;					// Instruction pointer
	page_directory_t *page_dir;
	u32int kernel_stack;
	s8int nice;
	u8int su;
	struct task *next;			// Next task in linked list
} task_t;

void init_tasking();
int switch_task();
void exit_task();
int fork();
int nice(int inc);
void move_stack(void *new_stack_start, u32int size);
int getpid();
void switch_user_mode();

#endif
