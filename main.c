/* main.c - start of C code; loaded by boot.s */
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

#include <multiboot.h>
#include <common.h>
#include <monitor.h>
#include <gdt.h>
#include <idt.h>
#include <string.h>
#include <paging.h>
#include <keyboard.h>
#include <timer.h>
#include <task.h>
#include <kmalloc.h>
#include <syscall.h>

extern void switch_user_mode();

// Defined in linker script
extern u32int kend;

u32int placement_address = (u32int)&kend;
u32int phys_address = (u32int)&kend - 0xC0000000u;
u32int initial_esp;

void kmain(u32int magic, multiboot_info_t *mboot, u32int esp) {
	initial_esp = esp;
	monitor_clear();
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		monitor_write("ERROR: MULTIBOOT_BOOTLOADER_MAGIC not correct. Halting.\n");
		halt();
	}

	init_gdt();
	init_idt();
	init_kbd();
	u32int mem_end = 0; // Last valid address in memory so we know how far to page

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
		monitor_write("Error! MULTIBOOT_INFO_MEM_MAP not set in mboot->flags.\n");
		halt();
	}

	init_paging(mem_end);
	init_timer(50);
	init_tasking();
	int ret = fork();
	if (ret == 0)
		monitor_write("Child\n");
	else {
		monitor_write("Parent\n");
	}

	while (1) {
		monitor_write_udec(ret);
		monitor_put('\n');
	}
	halt();
}
