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
#include <paging.h>
#include <chardev/term.h>
#include <timer.h>
#include <task.h>
#include <kmalloc.h>
#include <syscall.h>
#include <time.h>
#include <pci.h>
#include <fs/rootfs.h>
#include <dev.h>
#include <vfs.h>
#include <ide.h>

#include <string.h>

extern void switch_user_mode(void);

// Defined in linker script
extern u32int kend;

extern time_t current_time;

u32int placement_address = (u32int)&kend;
u32int phys_address = (u32int)&kend - 0xC0000000u;
u32int initial_esp;

void kmain(u32int magic, multiboot_info_t *mboot, u32int esp) {
	initial_esp = esp;
	u32int mem_end = 0; // Last valid address in memory so we know how far to page
	monitor_clear();
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		monitor_write("ERROR: MULTIBOOT_BOOTLOADER_MAGIC not correct. Halting.\n");
		halt();
	}

	init_gdt();
	init_idt();
	init_time();
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
		monitor_write("Error! MULTIBOOT_INFO_MEM_MAP not set in mboot->flags.\n");
		halt();
	}

	init_paging(mem_end);
	init_tasking();
	init_syscalls();

	dump_pci();
	init_rootfs();
	init_devfs();
	init_ide(0, 0, 0, 0, 0);

	mount(NULL, vfs_root, "rootfs", 0);
	fs_node_t *dev = finddir_vfs(vfs_root, "dev");
	mount(NULL, dev, "dev", 0);
	kfree(dev);
	devfs_register("sda0", VFS_FILE | VFS_BLOCKDEV, 1, 1, 0, 0, 0);
	int fd = sys_open("/dev/sda0", O_RDWR);
	monitor_write_sdec(fd);
	monitor_put('\n');
	char buf[1024];
	sys_pread(fd, buf, 1024, 12);
	buf[0] = 0xDE;
	buf[1] = 0xAD;
	buf[2] = 0xBE;
	buf[3] = 0xEF;
	sys_pwrite(fd, buf, 1024, 12);
	monitor_write_hex((u32int)buf);

	halt();
}
