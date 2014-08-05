/* elf.c - load and execute ELF files */

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

#include <elf.h>
#include <common.h>
#include <vfs.h>
#include <paging.h>
#include <kmalloc.h>
#include <task.h>
#include <string.h>
#include <errno.h>

// Defined in paging.c
extern page_directory_t *current_dir;

// Defined in task.c
extern task_t *current_task;

static int int_exec(const char *filename, uint32_t argc, char *argv[],
		uint32_t envc, char *envp[]) {
	fs_node_t *file = get_path(filename);
	if (!file)
		return -EACCES;

	if (file->mask & VFS_SETUID)
		current_task->euid = file->uid;

	if (file->mask & VFS_SETGID)
		current_task->euid = file->gid;

	if (open_vfs(file, O_RDONLY))
		goto error;

	Elf32_Ehdr *header = (Elf32_Ehdr*)kmalloc(sizeof(Elf32_Ehdr));
	if (!header)
		goto error;

	read_vfs(file, header, sizeof(Elf32_Ehdr), 0);

	/* Ugly-ass if block, but we have to check it all anyway or face the
	 * consequences, and we handle any part of this not matching up the
	 * exact same way, might as well do it now.
	 */
	if (header->e_ident[0] != ELFMAG0 ||
			header->e_ident[1] != ELFMAG1 ||
			header->e_ident[2] != ELFMAG2 ||
			header->e_ident[3] != ELFMAG3 ||
			header->e_ident[EI_CLASS] != ELFCLASS32 ||
			header->e_ident[EI_DATA] != ELFDATA2LSB ||
			header->e_type != ET_EXEC ||
			header->e_machine != EM_386 ||
			header->e_version != EV_CURRENT) {
		close_vfs(file);
		kfree(file);
		kfree(header);
		return -ENOEXEC;
	}

	// Get program headers and load them into memory
	Elf32_Phdr *prog_headers =
		(Elf32_Phdr*)kmalloc(header->e_phnum * header->e_phentsize);
	if (!prog_headers)
		goto error2;

	read_vfs(file, prog_headers, header->e_phnum * header->e_phentsize,
			header->e_phoff);

	// Deallocate memory of old process (not stack, we reuse it)
	uint32_t i;
	for (i = current_task->start; i < current_task->brk_actual; i += 0x1000)
		free_frame(get_page(i, 0, current_dir));

	// Allocate memory for new process
	uint32_t start = 0xFFFFFFFF;
	uint32_t size = 0;
	uint32_t entry = header->e_entry;

	for (i = 0; i < header->e_phnum; i++) {
		if (prog_headers[i].p_type != PT_LOAD)
			continue;

		if (prog_headers[i].p_vaddr < start)
			start = prog_headers[i].p_vaddr;

		uint32_t j;
		for (j = prog_headers[i].p_vaddr;
				j < prog_headers[i].p_vaddr + prog_headers[i].p_memsz;
				j += 0x1000)
			alloc_frame(get_page(j, 1, current_dir), 0,
					(prog_headers[i].p_flags & PF_W) ? 1 : 0, 0);

		switch_page_dir(current_dir);

		size += prog_headers[i].p_memsz;

		void *seg_start = (void*)prog_headers[i].p_vaddr;
		read_vfs(file, seg_start, prog_headers[i].p_filesz,
				prog_headers[i].p_offset);
	}

	close_vfs(file);
	kfree(file);
	kfree(header);
	kfree(prog_headers);

	current_task->start = start;

	uint32_t heap = start + size;
	alloc_frame(get_page(heap, 1, current_dir), 0, 1, 0);
	uint32_t heap_actual = heap + 0x1000;
	switch_page_dir(current_dir);

	// Move everything to user space
	char **argv_ = (char**)heap;
	heap += sizeof(char*) * (argc + 1);
	char **envp_ = (char**)heap;

	for (i = 0; i < argc; i++) {
		argv_[i] = (char*)heap;
		memcpy((void*)heap, argv[i], strlen(argv[i]) + 1);
		heap += strlen(argv[i]) + 1;
		kfree(argv[i]);
	}
	argv_[i] = NULL;
	heap += 1;
	kfree(argv);

	for (i = 0; i < envc; i++) {
		envp_[i] = (char*)heap;
		memcpy((void*)heap, envp[i], strlen(envp[i]) + 1);
		heap += strlen(argv[i]) + 1;
		kfree(envp[i]);
	}
	envp_[i] = NULL;
	heap += 1;
	kfree(envp);

	current_task->brk = (uint32_t)heap;
	current_task->brk_actual = (uint32_t)heap_actual;

	switch_user_mode(entry, argc, argv_, envp_, USER_STACK_TOP);
error2:
	kfree(header);
error:
	kfree(file);
	return -ENOMEM;
}

int execve(const char *filename, char *const argv[], char *const envp[]) {
	if (!filename)
		return -EFAULT;

	if (!argv)
		return -EFAULT;

	if (!envp)
		return -EFAULT;

	uint32_t argc;
	uint32_t envc;
	for (argc = 0; argv[argc]; argc++);
	for (envc = 0; envp[envc]; envc++);

	// Move everything to kernel space
	char **argv_ = (char**)kmalloc(sizeof(char*) * (argc + 1));
	char **envp_ = (char**)kmalloc(sizeof(char*) * (envc + 1));

	uint32_t i;
	for (i = 0; i < argc; i++) {
		argv_[i] = (char *)kmalloc(strlen(argv[i]) + 1);
		memcpy(argv_[i], argv[i], strlen(argv[i]) + 1);
	}
	argv_[argc] = NULL;
	for (i = 0; i < envc; i++) {
		envp_[i] = (char *)kmalloc(strlen(envp[i]) + 1);
		memcpy(envp_[i], envp[i], strlen(envp[i]) + 1);
	}
	envp_[envc] = NULL;

	return int_exec(filename, argc, argv_, envc, envp_);
}
