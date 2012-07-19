/* pci.c - read and write PCI registers */
/* Copyright (C) 2011, 2012 Bth8 <bth8fwd@gmail.com>
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
#include <pci.h>
#include <pci_regs.h>
#include <printf.h>

u32int pciConfigReadDword(u8int bus, u8int slot, u8int func, u8int off) {
	// Select config address
	outl(CONFIG_ADDRESS, (bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	// Read in data
	return inl(CONFIG_DATA);
}

u16int pciConfigReadWord(u8int bus, u8int slot, u8int func, u8int off) {
	outl(CONFIG_ADDRESS, (bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	return inw(CONFIG_DATA + (off & 2));
}

u8int pciConfigReadByte(u8int bus, u8int slot, u8int func, u8int off) {
	outl(CONFIG_ADDRESS, (bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	return inb(CONFIG_DATA + (off & 3));
}

void pciConfigWriteDword(u8int bus, u8int slot, u8int func, u8int off, u32int val) {
	outl(CONFIG_ADDRESS, (bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	outl(CONFIG_DATA, val);
}

void pciConfigWriteWord(u8int bus, u8int slot, u8int func, u8int off, u16int val) {
	outl(CONFIG_ADDRESS, (bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	outw(CONFIG_DATA + (off & 2), val);
}

void pciConfigWriteByte(u8int bus, u8int slot, u8int func, u8int off, u8int val) {
	outl(CONFIG_ADDRESS, (bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	outb(CONFIG_DATA + (off & 3), val);
}


void dump_pci(void) {
	u16int bus, slot, func;
	for (bus = 0; bus < 256; bus++)
		for (slot = 0; slot < 32; slot++)
			for (func = 0; func < 8; func++) {
				int vend = pciConfigReadWord(bus, slot, func, PCI_VENDOR_ID);
				if (vend != 0xFFFF)
					printf("Bus %u Slot %u Func %u - Device: 0x%X vendor: 0x%X class: 0x%X\n",
							bus, slot, func, pciConfigReadWord(bus, slot, func, PCI_DEVICE_ID),
							vend, pciConfigReadWord(bus, slot, func, PCI_CLASS_DEVICE));
			}
}
