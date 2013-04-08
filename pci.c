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
#include <kmalloc.h>

struct pci_bus bus0;
struct pci_dev host;

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

static u8int fill_bus(struct pci_bus *parent, struct pci_dev *busdev);

static u8int check_func(struct pci_bus *bus, u8int slot, u8int func) {
	struct pci_dev *iter = bus->devs;
	u16int vend = pciConfigReadWord(bus->secondary, slot, func, PCI_VENDOR_ID);	// Make sure it's a valid function
	if (vend != 0xFFFF) {
		struct pci_dev *dev = (struct pci_dev*)kmalloc(sizeof(struct pci_dev));
		if (dev == NULL)
			return -1;

		dev->bus = bus;
		dev->sibling = NULL;
		dev->next = NULL;
		dev->slot = slot;
		dev->func = func;
		dev->vendor = vend;
		dev->device = pciConfigReadWord(bus->secondary, slot, func, PCI_DEVICE_ID);
		dev->class = pciConfigReadDword(bus->secondary, slot, func, PCI_CLASS_REVISION) >> 8;

		if (iter == NULL) {				// First device on bus
			bus->devs = dev;
			iter = bus->self;			// So we have a valid device in the chain
		} else {
			while (iter->sibling != NULL)	// Get last device in bus
				iter = iter->sibling;

			iter->sibling = dev;
		}

		while (iter->next != NULL)
			iter = iter->next;

		iter->next = dev;

		if ((dev->class >> 8) == 0x604)
			return fill_bus(bus, dev);
	}
	return 0;
}

static u8int check_device(struct pci_bus *bus, u8int slot) {
	u8int func = 0;

	u8int total = 0;

	u16int vend = pciConfigReadWord(bus->secondary, slot, func, PCI_VENDOR_ID);
	if (vend == 0xFFFF) return 0;
	u8int header = pciConfigReadByte(bus->secondary, slot, func, PCI_HEADER_TYPE);
	if (header & 0x80) { // multifunction device
		for (; func < 8; func++)
			total += check_func(bus, slot, func);
	} else
		total = check_func(bus, slot, func);

	return total;
}

static u8int check_bus(struct pci_bus *bus) {
	// kinda kludge-y, but I'm gonna do it fuck you
	u8int slot = (bus->secondary == 0) ? 1 : 0;
	u8int total = 0;

	for (; slot < 32; slot++)
		total += check_device(bus, slot);

	return total;
}

static u8int fill_bus(struct pci_bus *parent, struct pci_dev *busdev) {
	struct pci_bus *iter = parent;
	struct pci_bus *bus = (struct pci_bus *)kmalloc(sizeof(struct pci_bus));
	if (bus == NULL)
		return -1;

	bus->parent = parent;
	bus->children = NULL;
	bus->sibling = NULL;
	bus->next = NULL;
	bus->self = busdev;
	bus->devs = NULL;

	while (iter->next != NULL)
		iter = iter->next;		// get the last bus found

	bus->secondary = iter->secondary + 1;	// assign bus number of last found + 1
	bus->primary = parent->secondary;
	bus->subordinate = 0xFF;				// We don't know yet

	// Configure the actual device to behave correctly
	pciConfigWriteByte(busdev->bus->secondary, busdev->slot, busdev->func, PCI_PRIMARY_BUS, bus->primary);
	pciConfigWriteByte(busdev->bus->secondary, busdev->slot, busdev->func, PCI_SECONDARY_BUS, bus->secondary);
	pciConfigWriteByte(busdev->bus->secondary, busdev->slot, busdev->func, PCI_SUBORDINATE_BUS, bus->subordinate);

	if (parent->children == NULL)
		parent->children = bus;
	else {
		struct pci_bus *tmp = iter;
		iter = parent->children;
		while (iter->sibling != NULL)
			iter = iter->sibling;

		iter->sibling = bus;
		iter = tmp;
	}

	iter->next = bus;

	u8int nbelow = check_bus(bus);
	bus->subordinate = bus->secondary + nbelow;
	pciConfigWriteByte(busdev->bus->secondary, busdev->slot, busdev->func, PCI_SUBORDINATE_BUS, bus->subordinate);
	return nbelow;
}

void init_pci(void) {
	bus0.parent = NULL;
	bus0.children = NULL;
	bus0.sibling = NULL;
	bus0.next = NULL;
	bus0.self = &host;
	bus0.devs = NULL;
	bus0.primary = 0;
	bus0.secondary = 0;
	bus0.subordinate = 0xFF;

	host.bus = &bus0;
	host.sibling = NULL;
	host.next = NULL;
	host.slot = 0;
	host.func = 0;
	host.vendor = pciConfigReadWord(0, 0, 0, PCI_VENDOR_ID);
	host.class = 0x60000;

	// and awaaaaaaaay we go!
	bus0.subordinate = check_bus(&bus0);
}

void dump_pci(void) {
	struct pci_dev *iter = &host;

	while (iter != NULL) {
		printf("Bus %u Slot %u Func %u:\n\tVendor: 0x%X\n\tDevice: 0x%X\n\tClass: 0x%06X\n",
				iter->bus->secondary, iter->slot, iter->func, iter->vendor,
				iter->device, iter->class);

		iter = iter->next;
	}

}
