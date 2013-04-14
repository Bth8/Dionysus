/* task.c - C functions for process creation and context switching. Also
 * switches stack from the minimal one set up in boot.s and provides several
 * user functions
 */
/* Copyright (C) 2011-2013 Bth8 <bth8fwd@gmail.com>
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

// Defined in main.c
extern u32int initial_esp;

// Defined in process.s
extern u32int read_eip(void);

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

	off_t offset = (u32int)new_stack_start - initial_esp;
	u32int new_esp = old_esp + offset;
	u32int new_ebp = old_ebp + offset;

	// Copy old stack contents into new one
	memcpy((void *)new_esp, (void *)old_esp, initial_esp - old_esp);

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

void init_tasking(void) {
	asm volatile("cli");
	int i;
	// Relocate stack
	move_stack((void *)0xF0000000, 0x2000);

	// Init first task (kernel task)
	current_task = ready_queue = (task_t *)kmalloc(sizeof(task_t));
	current_task->id = next_pid++;
	current_task->esp = current_task->ebp = 0;
	current_task->eip = 0;
	current_task->page_dir = current_dir;
	current_task->kernel_stack = (u32int)kmalloc_a(KERNEL_STACK_SIZE);
	current_task->nice = 0;
	current_task->euid, current_task->suid, current_task->ruid = 0;
	current_task->egid, current_task->rgid, current_task->sgid = 0;
	current_task->next = NULL;

	// No open files yet
	for (i = 0; i < MAX_OF; i++)
		current_task->files[i].file = NULL;

	asm volatile("sti");
}

int fork(void) {
	asm volatile("cli");
	int i;
	page_directory_t *directory = clone_directory(current_dir);

	// Create a new process
	task_t *new_task = (task_t *)kmalloc(sizeof(task_t));
	new_task->id = next_pid++;
	new_task->esp = new_task->ebp = 0;
	new_task->eip = 0;
	new_task->page_dir = directory;
	new_task->kernel_stack = (u32int)kmalloc_a(KERNEL_STACK_SIZE);
	// Inherit niceness and ids of parent
	new_task->nice = current_task->nice;
	new_task->euid = current_task->euid;
	new_task->ruid = current_task->ruid;
	new_task->suid = current_task->suid;
	new_task->egid = current_task->egid;
	new_task->rgid = current_task->rgid;
	new_task->sgid = current_task->sgid;
	new_task->next = NULL;

	// Copy open files
	for (i = 0; i < MAX_OF; i++) {
		if (current_task->files[i].file != NULL) {
			new_task->files[i].file = (fs_node_t *)kmalloc(sizeof(fs_node_t));
			memcpy(new_task->files[i].file, current_task->files[i].file, sizeof(fs_node_t));
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
		// Send EOI to PIC. Otherwise, PIT won't fire again.
		outb(0x20, 0x20);
		return 0;
	}
}

int getpid(void) {
	return current_task->id;
}

int switch_task(void) {
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
	return 0;
}

void exit_task(void) {
	asm volatile("cli");
	int i;
	task_t *task_i = (task_t *)ready_queue, *current_cache = (task_t *)current_task;

	// Make sure we're not the only task
	ASSERT(task_i->next != NULL);

	// Not the first task in the run queue
	if (task_i != current_task) {
		// Find the previous task
		while (task_i->next != current_task)
			task_i = task_i->next;
		// Point it to the next one. We're out of the run queue
		task_i->next = current_task->next;
	} else {
		// Move the run queue to its second task
		task_i = task_i->next;
		ready_queue = task_i;
	}
	// Prepare for a context switch
	current_task = task_i;
	current_dir = current_task->page_dir;
	set_kernel_stack(current_task->kernel_stack + KERNEL_STACK_SIZE);
	// Use new current_task's paging dir, because we're about to trash ours
	asm volatile("mov %0, %%cr3":: "r"(current_dir->physical_address));

	// Free everything
	for (i = 0; i < MAX_OF; i++)
		if (current_cache->files[i].file != NULL)
			kfree(current_cache->files[i].file);
	free_dir(current_cache->page_dir);
	kfree((void *)current_cache->kernel_stack);
	kfree((void *)current_cache);

	// Context switching time
	asm volatile("mov %0, %%ecx;\
				mov %1, %%esp;\
				mov %2, %%ebp;\
				mov $0x12345, %%eax;\
				sti;\
				jmp *%%ecx" :: "r"(current_task->eip), "r"(current_task->esp), "r"(current_task->ebp) : "ecx", "esp", "ebp", "eax");
}	

void switch_user_mode(void) {
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

int nice(int inc) {
	if (inc < 0 && !current_task->euid)
		return -1;
	if (current_task->nice + inc > 19)
		current_task->nice = 19;
	else if (current_task->nice + inc < -20)
		current_task->nice = -20;
	else
		current_task->nice += inc;
	return current_task->nice;
}

int seteuid(int new_euid) {
	int suid = current_task->euid;
	if (current_task->euid)
		if (new_euid == current_task->suid || new_euid == current_task->ruid || new_euid == current_task->euid)
			current_task->euid = new_euid;
		else
			return -1;
	else
		current_task->euid = new_euid;

	current_task->suid = suid;
	return 0;
}

int setreuid(int new_ruid, int new_euid) {
	int suid = current_task->euid;
	if (current_task->euid) {
		if (new_ruid > -1) {
			if (new_ruid == current_task->euid || new_ruid == current_task->ruid)
				current_task->ruid = new_ruid;
			else
				return -1;
		}
		if (new_euid > -1 && seteuid(new_euid) < 0)
			return -1;
	} else {
		if (new_ruid > -1)
			current_task->ruid = new_ruid;
		if (new_euid > -1)
			current_task->euid = new_euid;
	}

	current_task->suid = suid;
	return 0;
}			

int setresuid(int new_ruid, int new_euid, int new_suid) {
	if (current_task->euid) {
		if (new_suid > -1) {
			if (new_suid == current_task->euid || new_suid == current_task->ruid || new_suid == current_task->suid)
				current_task->suid = new_suid;
			else
				return -1;
		}
		if ((new_euid > -1 || new_ruid > -1) && setreuid(new_ruid, new_euid))
			return -1;
	} else {
		if (new_ruid > -1)
			current_task->ruid = new_ruid;
		if (new_euid > -1)
			current_task->euid = new_euid;
		if (new_suid > -1)
			current_task->suid = new_suid;
	}

	return 0;
}

int setuid(int uid) {
	if (!current_task->euid)
		current_task->ruid = current_task->suid = current_task->euid = uid;
	else
		return seteuid(uid);

	return 0;
}

int getuid(void) {
	return current_task->ruid;
}

int geteuid(void) {
	return current_task->euid;
}

int getresuid(int *ruid, int *euid, int *suid) {
	*ruid = current_task->ruid;
	*euid = current_task->euid;
	*suid = current_task->suid;
	return 0;
}

int setegid(int new_egid) {
	int sgid = current_task->egid;
	if (current_task->egid)
		if (new_egid == current_task->sgid || new_egid == current_task->rgid || new_egid == current_task->egid)
			current_task->egid = new_egid;
		else
			return -1;
	else
		current_task->egid = new_egid;

	current_task->sgid = sgid;
	return 0;
}

int setregid(int new_rgid, int new_egid) {
	int sgid = current_task->egid;
	if (current_task->egid) {
		if (new_rgid > -1) {
			if (new_rgid == current_task->egid || new_rgid == current_task->rgid)
				current_task->rgid = new_rgid;
			else
				return -1;
		}
		if (new_egid > -1 && setegid(new_egid) < 0)
			return -1;
	} else {
		if (new_rgid > -1)
			current_task->rgid = new_rgid;
		if (new_egid > -1)
			current_task->egid = new_egid;
	}

	current_task->sgid = sgid;
	return 0;
}			

int setresgid(int new_rgid, int new_egid, int new_sgid) {
	if (current_task->egid) {
		if (new_sgid > -1) {
			if (new_sgid == current_task->egid || new_sgid == current_task->rgid || new_sgid == current_task->sgid)
				current_task->sgid = new_sgid;
			else
				return -1;
		}
		if ((new_egid > -1 || new_rgid > -1) && setregid(new_rgid, new_egid))
			return -1;
	} else {
		if (new_rgid > -1)
			current_task->rgid = new_rgid;
		if (new_egid > -1)
			current_task->egid = new_egid;
		if (new_sgid > -1)
			current_task->sgid = new_sgid;
	}

	return 0;
}

int setgid(int gid) {
	if (!current_task->egid)
		current_task->rgid = current_task->sgid = current_task->egid = gid;
	else
		return setegid(gid);

	return 0;
}

int getgid(void) {
	return current_task->rgid;
}

int getegid(void) {
	return current_task->egid;
}

int getresgid(int *rgid, int *egid, int *sgid) {
	*rgid = current_task->rgid;
	*egid = current_task->egid;
	*sgid = current_task->sgid;
	return 0;
}

static int valid_fd(int fd) {
	if (fd < 0)
		return 0;
	if (fd > MAX_OF)
		return 0;
	if (!current_task->files[fd].file)
		return 0;

	return 1;
}

int lseek(int fd, off_t off, int whence) {
	if (!valid_fd(fd))
		return -1;

	switch (whence) {
		case SEEK_SET:
			current_task->files[fd].off = off;
			break;
		case SEEK_CUR:
			current_task->files[fd].off += off;
			break;
		case SEEK_END:
			current_task->files[fd].off = current_task->files[fd].file->len + off;
			break;
		default:
			return -1;
	}

	return current_task->files[fd].off;
}

int user_pread(int fd, char *buf, size_t nbytes, off_t off) {
	if (!valid_fd(fd))
		return -1;

	if (!buf)
		return -1;

	if (!(current_task->files[fd].file->mask & O_RDONLY))
		return -1;

	return read_vfs(current_task->files[fd].file, buf, nbytes, off);
}

int user_read(int fd, char *buf, size_t nbytes) {
	if (!valid_fd(fd))
		return -1;

	if (!buf)
		return -1;

	if (!(current_task->files[fd].file->mask & O_RDONLY))
		return -1;

	u32int ret = read_vfs(current_task->files[fd].file, buf, nbytes, current_task->files[fd].off);
	current_task->files[fd].off += ret;
	return ret;
}

int user_pwrite(int fd, const char *buf, size_t nbytes, off_t off) {
	if (!valid_fd(fd))
		return -1;

	if (!buf)
		return -1;

	if (!(current_task->files[fd].file->mask & O_WRONLY))
		return -1;

	return write_vfs(current_task->files[fd].file, buf, nbytes, off);
}

int user_write(int fd, const char *buf, size_t nbytes) {
	if (!valid_fd(fd))
		return -1;

	if (!buf)
		return -1;

	if (!(current_task->files[fd].file->mask & O_WRONLY))
		return -1;

	u32int ret = write_vfs(current_task->files[fd].file, buf, nbytes, current_task->files[fd].off);
	current_task->files[fd].off += ret;
	return ret;
}

static int check_flags(fs_node_t *file, u32int flags) {
	int acceptable = 1;

	// Root is all powerful
	if (current_task->euid == 0)
		return acceptable;

	if (flags & O_RDONLY) {
		if (!(file->mask & VFS_O_READ)) {
			acceptable = 0;
			if (current_task->euid == file->uid && file->mask & VFS_U_READ)
				acceptable = 1;
			else if (current_task->egid == file->gid && file->mask & VFS_G_READ)
				acceptable = 1;
		}
	}

	if (flags & O_WRONLY && acceptable == 1) {
		if (!(file->mask & VFS_O_WRITE)) {
			acceptable = 0;
			if (current_task->euid == file->uid && file->mask & VFS_U_WRITE)
				acceptable = 1;
			else if (current_task->egid == file->gid && file->mask & VFS_G_WRITE)
				acceptable = 1;
		}
	}

	return acceptable;
}
		

int user_open(const char *path, u32int flags, u32int mode) {
	if (!path)
		return -1;

	int i;
	for (i = 0; i < MAX_OF; i++)
		if (current_task->files[i].file == NULL)
			break;

	if (i == MAX_OF)
		return -1;

	int ret = -1;
	fs_node_t *file = get_path(path);
	if (file && !(flags & O_EXCL) && check_flags(file, flags)) {
		if ((ret = open_vfs(file, flags)) == 0) {
			current_task->files[i].file = file;
			current_task->files[i].off = 0;
			if (flags & O_APPEND)
				lseek(i, 0, SEEK_END);
			return i;
		}
	} else if (!file && (flags & O_CREAT)) {
		file = create_vfs(path, current_task->euid, current_task->egid, mode);
		if (file != NULL) {
			current_task->files[i].file = file;
			current_task->files[i].off = 0;
			return i;
		}
	}

	kfree(file);
	return ret;
}

int user_close(int fd) {
	if (!valid_fd(fd))
		return -1;

	u32int ret = close_vfs(current_task->files[fd].file);
	if (ret == 0) {
		kfree(current_task->files[fd].file);
		current_task->files[fd].file = NULL;
	}
	return ret;
}

int user_ioctl(int fd, u32int request, void *ptr) {
	if (!valid_fd(fd))
		return -1;

	return ioctl_vfs(current_task->files[fd].file, request, ptr);
}

int user_mount(const char *src, const char *target, const char *fs_name, u32int flags) {
	if (!target || !fs_name)
		return -1;

	fs_node_t *src_node = get_path(src);
	if (src && !src_node)
		return -1;

	fs_node_t *dest_node = get_path(target);
	if (!dest_node) {
		kfree(src_node);
		return -1;
	}

	u32int ret = mount(src_node, dest_node, fs_name, flags);

	kfree(src_node);
	kfree(dest_node);
	return ret;
}

int user_readdir(int fd, struct dirent *dirp, u32int index) {
	if (!valid_fd(fd))
		return -1;

	if (!dirp)
		return -1;

	return readdir_vfs(current_task->files[fd].file, dirp, index);
}
