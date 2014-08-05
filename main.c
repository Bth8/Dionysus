/* main.c - start of C code; loaded by boot.s */

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

#include <multiboot.h>
#include <common.h>
#include <monitor.h>
#include <gdt.h>
#include <idt.h>
#include <paging.h>
#include <chardev/term.h>
#include <timer.h>
#include <task.h>
#include <syscall.h>
#include <pci.h>
#include <fs/rootfs.h>
#include <dev.h>
#include <vfs.h>
#include <ide.h>
#include <printf.h>
#include <fs/fat32.h>
#include <elf.h>

// Defined in linker script
extern uint32_t kend;

extern time_t current_time;

uint32_t placement_address = (uint32_t)&kend;
uint32_t phys_address = (uint32_t)&kend - 0xC0000000u;
uint32_t initial_esp;

void print_time(struct tm *time);

void kmain(uint32_t magic, multiboot_info_t *mboot, uint32_t esp) {
	initial_esp = esp;
	monitor_clear();
	printf("Booting Dionysus!\n");
	ASSERT(magic == MULTIBOOT_BOOTLOADER_MAGIC &&
			"Not booted with multiboot.");
	ASSERT(mboot->flags & MULTIBOOT_INFO_MEMORY && "No memory info.");
	ASSERT(mboot->flags & MULTIBOOT_INFO_MEM_MAP && "No memory map.");

	printf("Initializing GDT\n");
	init_gdt();

	printf("Initializing IDT\n");
	init_idt();
	
	printf("Getting system time: ");
	init_time();
	
	time_t rawtime = time(NULL);
	print_time(gmtime(&rawtime));
	init_timer(1000);

	// Check for modules
	if (mboot->flags & MULTIBOOT_INFO_MODS && mboot->mods_count) {
		multiboot_module_t *mods = (multiboot_module_t *)mboot->mods_addr;
		phys_address = mods[mboot->mods_count - 1].mod_end;
		placement_address = phys_address + 0xC0000000u;
	}

	multiboot_memory_map_t *mmap = (void *)mboot->mmap_addr;

	while ((uint32_t)mmap < mboot->mmap_addr + mboot->mmap_length) {
		printf("0x%08X 0x%08X-0x%08X: %d\n", mmap, mmap->addr_low,
				mmap->addr_low + mmap->len_low, mmap->type);
		mmap = (void*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
	}

	printf("Setting up paging\n");
	init_paging(mboot->mem_lower + mboot->mem_upper);

	printf("Starting task scheduling\n");
	init_tasking();
	init_syscalls();

	init_pci();
	dump_pci();
	init_rootfs();
	init_devfs();
	init_term();
	init_ide(0, 0, 0, 0, 0);

	mount(NULL, vfs_root, "rootfs", 0);
	user_mount(NULL, "/dev", "dev", 0);

	init_fat32();

	devfs_register("hda", VFS_BLOCKDEV, IDE_MAJOR, 0, 0, 0, 0);
	devfs_register("hda1", VFS_BLOCKDEV, IDE_MAJOR, 1, 0, 0, 0);
	devfs_register("hda2", VFS_BLOCKDEV, IDE_MAJOR, 2, 0, 0, 0);
	devfs_register("tty", VFS_CHARDEV, 1, 0, VFS_O_READ | VFS_O_WRITE, 0, 0);

	user_mount("/dev/hda2", "/real_root", "fat32", 0);

	user_open("/dev/tty", O_RDWR, 0);
	user_open("/dev/tty", O_RDWR, 0);
	user_open("/dev/tty", O_RDWR, 0);

	int pid = sys_fork();
	char *argv[] = {NULL};
	char *envp[] = {NULL};
	if (pid == 0)
		sys_execve("/real_root/init", argv, envp);

	halt();
}

void print_time(struct tm *time) {
	printf("%s, ", (char *[]){"Sunday", "Monday", "Tuesday", "Wednesday",
			"Thursday", "Friday", "Saturday"}[time->tm_wday]);
	printf("%s ", (char *[]){"Jan", "Feb", "March", "April", "May", "June",
			"July", "Aug", "Sep", "Oct", "Nov", "Dec"}[time->tm_mon]);
	printf("%i %02i:%02i:%02i %i\n", time->tm_mday, time->tm_hour,
			time->tm_min, time->tm_sec, time->tm_year + 1900);
}
