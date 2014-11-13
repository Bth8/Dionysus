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

#include <common.h>
#include <monitor.h>
#include <printf.h>
#include <multiboot.h>
#include <gdt.h>
#include <idt.h>
#include <time.h>
#include <timer.h>
#include <paging.h>
#include <task.h>
#include <syscall.h>
#include <vfs.h>
#include <block.h>
#include <char.h>
#include <fs/dev.h>
#include <pci.h>

// LOOK HERE
// Defined in linker script
extern uintptr_t kend;

extern time_t current_time;

uintptr_t placement_address = (uint32_t)&kend;

void print_time(struct tm *time);

void kmain(uint32_t magic, multiboot_info_t *mboot, uintptr_t ebp) {
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
	init_timer();

	// Check for modules
	if (mboot->flags & MULTIBOOT_INFO_MODS && mboot->mods_count) {
		multiboot_module_t *mods = (multiboot_module_t *)mboot->mods_addr;
		placement_address = mods[mboot->mods_count - 1].mod_end + KERNEL_BASE;
	}

	printf("Setting up paging\n");
	init_paging(mboot->mem_lower + mboot->mem_upper, mboot->mmap_addr,
		mboot->mmap_length);

	printf("Starting task scheduling\n");
	init_tasking(ebp);
	init_syscalls();

	printf("Initializing vfs\n");
	init_vfs();

	printf("Initializing driver subsystem\n");
	init_blockdev();
	init_chardev();

	printf("Enumerating PCI bus(ses)\n");
	init_pci();
	dump_pci();

	init_devfs();
	ASSERT(mount(NULL, "/dev", "devfs", 0) == 0);

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
