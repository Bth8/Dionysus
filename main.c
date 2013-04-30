/* main.c - start of C code; loaded by boot.s */
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
extern u32int kend;

extern time_t current_time;

u32int placement_address = (u32int)&kend;
u32int phys_address = (u32int)&kend - 0xC0000000u;
u32int initial_esp;

void print_time(struct tm *time);

void kmain(u32int magic, multiboot_info_t *mboot, u32int esp) {
	initial_esp = esp;
	u32int mem_end = 0; // Last valid address in memory so we know how far to page
	monitor_clear();
	printf("Booting Dionysus!\n");
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		printf("ERROR: MULTIBOOT_BOOTLOADER_MAGIC not correct. Halting.\n");
		halt();
	}

	printf("Initializing GDT\n");
	init_gdt();
	printf("Initializing IDT\n");
	init_idt();
	printf("Getting system time: ");
	init_time();
	time_t rawtime = time(NULL);
	print_time(gmtime(&rawtime));
	init_timer(1000);

	if (mboot->flags & MULTIBOOT_INFO_MODS && mboot->mods_count) {
		multiboot_module_t *mods = (multiboot_module_t *)mboot->mods_addr;
		phys_address = mods[mboot->mods_count - 1].mod_end;
		placement_address = mods[mboot->mods_count - 1].mod_end + 0xC0000000u;
	}
	if (mboot->flags & MULTIBOOT_INFO_MEM_MAP) {
		multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)mboot->mmap_addr;
		while ((u32int)mmap < mboot->mmap_addr + mboot->mmap_length) {
			if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
				if (mmap->addr_low + mmap->len_low <= 4294967295U)
					mem_end = mmap->addr_low + mmap->len_low;
				else {
					mem_end = 4294967295U;
					break;
				}
			}

			mmap = (multiboot_memory_map_t *)((u32int)mmap + mmap->size + sizeof(mmap->size));
		}
	} else {
		printf("Error! MULTIBOOT_INFO_MEM_MAP not set in mboot->flags.\n");
		halt();
	}

	init_paging(mem_end);
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
