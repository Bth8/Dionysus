/* pci.c - read and write PCI registers */

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
#include <pci.h>
#include <pci_regs.h>
#include <pci_ids.h>
#include <printf.h>
#include <kmalloc.h>
#include <errno.h>
#include <structures/list.h>
#include <string.h>

struct pci_bus bus0;
struct pci_dev host;

list_t *drivers;

inline uint32_t pciConfigReadDword(uint8_t bus, uint8_t slot, uint8_t func,
		uint8_t off) {
	// Select config address
	outl(CONFIG_ADDRESS,
			(bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	// Read in data
	return inl(CONFIG_DATA);
}

inline uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func,
		uint8_t off) {
	outl(CONFIG_ADDRESS,
			(bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	return inw(CONFIG_DATA + (off & 2));
}

inline uint8_t pciConfigReadByte(uint8_t bus, uint8_t slot, uint8_t func,
		uint8_t off) {
	outl(CONFIG_ADDRESS,
			(bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	return inb(CONFIG_DATA + (off & 3));
}

inline void pciConfigWriteDword(uint8_t bus, uint8_t slot, uint8_t func,
		uint8_t off, uint32_t val) {
	outl(CONFIG_ADDRESS,
			(bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	outl(CONFIG_DATA, val);
}

inline void pciConfigWriteWord(uint8_t bus, uint8_t slot, uint8_t func,
		uint8_t off, uint16_t val) {
	outl(CONFIG_ADDRESS,
			(bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	outw(CONFIG_DATA + (off & 2), val);
}

inline void pciConfigWriteByte(uint8_t bus, uint8_t slot, uint8_t func,
		uint8_t off, uint8_t val) {
	outl(CONFIG_ADDRESS,
			(bus << 16) | (slot << 11) | (func << 8) | off | 0x80000000);
	outb(CONFIG_DATA + (off & 3), val);
}

static uint8_t fill_bus(struct pci_bus *parent, struct pci_dev *busdev);

static uint8_t check_func(struct pci_bus *bus, uint8_t slot, uint8_t func) {
	struct pci_dev *iter = bus->devs;
	uint16_t vend =
		pciConfigReadWord(bus->secondary, slot, func, PCI_VENDOR_ID);
	// Make sure it's a valid function
	if (vend != 0xFFFF) {
		struct pci_dev *dev =
			(struct pci_dev*)kmalloc(sizeof(struct pci_dev));
		if (dev == NULL)
			return -ENOMEM;

		dev->bus = bus;
		dev->sibling = NULL;
		dev->next = NULL;
		dev->slot = slot;
		dev->func = func;
		dev->vendor = vend;
		dev->device =
			pciConfigReadWord(bus->secondary, slot, func, PCI_DEVICE_ID);
		dev->class =
			pciConfigReadWord(bus->secondary, slot, func, PCI_CLASS_DEVICE);
		dev->prog = 
			pciConfigReadWord(bus->secondary, slot, func, PCI_CLASS_PROG);
		dev->claimed = 0;

		if (iter == NULL) {
			bus->devs = dev;
			iter = bus->self;
		} else {
			// Seek to last device
			while (iter->sibling != NULL)
				iter = iter->sibling;

			iter->sibling = dev;
		}

		while (iter->next != NULL)
			iter = iter->next;

		iter->next = dev;

		if ((dev->class >> 8) == PCI_CLASS_BRIDGE_PCI)
			return fill_bus(bus, dev);
	}
	return 0;
}

static uint8_t check_device(struct pci_bus *bus, uint8_t slot) {
	uint8_t func = 0;

	uint8_t total = 0;

	uint16_t vend =
		pciConfigReadWord(bus->secondary, slot, func, PCI_VENDOR_ID);
	if (vend == 0xFFFF)
		return 0;
	uint8_t header =
		pciConfigReadByte(bus->secondary, slot, func, PCI_HEADER_TYPE);
	if (header & 0x80) { // multifunction device
		for (; func < 8; func++)
			total += check_func(bus, slot, func);
	} else
		total = check_func(bus, slot, func);

	return total;
}

static uint8_t check_bus(struct pci_bus *bus) {
	// kinda kludge-y, but I'm gonna do it fuck you
	uint8_t slot = (bus->secondary == 0) ? 1 : 0;
	uint8_t total = 0;

	for (; slot < 32; slot++)
		total += check_device(bus, slot);

	return total;
}

static uint8_t fill_bus(struct pci_bus *parent, struct pci_dev *busdev) {
	struct pci_bus *iter = parent;
	struct pci_bus *bus = (struct pci_bus *)kmalloc(sizeof(struct pci_bus));
	if (bus == NULL)
		return -ENOMEM;

	bus->parent = parent;
	bus->children = NULL;
	bus->sibling = NULL;
	bus->next = NULL;
	bus->self = busdev;
	bus->devs = NULL;

	while (iter->next != NULL)
		iter = iter->next;

	bus->secondary = iter->secondary + 1;
	bus->primary = parent->secondary;
	bus->subordinate = 0xFF;

	// Configure the actual device to behave correctly
	pciConfigWriteByte(busdev->bus->secondary, busdev->slot, busdev->func,
			PCI_PRIMARY_BUS, bus->primary);
	pciConfigWriteByte(busdev->bus->secondary, busdev->slot, busdev->func,
			PCI_SECONDARY_BUS, bus->secondary);
	pciConfigWriteByte(busdev->bus->secondary, busdev->slot, busdev->func,
			PCI_SUBORDINATE_BUS, bus->subordinate);

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

	uint8_t nbelow = check_bus(bus);
	bus->subordinate = bus->secondary + nbelow;
	pciConfigWriteByte(busdev->bus->secondary, busdev->slot, busdev->func,
			PCI_SUBORDINATE_BUS, bus->subordinate);
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
	host.class = PCI_CLASS_BRIDGE_HOST;

	bus0.subordinate = check_bus(&bus0);

	drivers = list_create();
	ASSERT(drivers);
}

void dump_pci(void) {
	struct pci_dev *iter = &host;

	while (iter != NULL) {
		printf("Bus %u Slot %u Func %u:\n\tVendor: 0x%04X\n\tDevice:"
				"0x%04X\n\tClass: 0x%04X\n",
				iter->bus->secondary, iter->slot, iter->func, iter->vendor,
				iter->device, iter->class);

		iter = iter->next;
	}

}

static void driver_search(struct pci_driver *driver) {
	ASSERT(driver);

	const struct pci_dev_id *id_iter = driver->table;
	while (id_iter->mask != 0) {
		struct pci_dev *dev_iter = &host;
		for (dev_iter = &host; dev_iter; dev_iter = dev_iter->next) {
			if (id_iter->vendor != PCI_ANY &&
					id_iter->vendor != dev_iter->vendor)
				continue;
			if (id_iter->device != PCI_ANY &&
					id_iter->device != dev_iter->device)
				continue;
			if (id_iter->class != PCI_ANY && 
					id_iter->class != (dev_iter->class & id_iter->mask))
				continue;
			while (driver->probe(dev_iter, id_iter) == -ENOMEM)
				continue;
		}
	}
}

spinlock_t pci_lock = 0;

int32_t register_pci(const char *name, const struct pci_dev_id *table,
		int (*probe)(struct pci_dev*, const struct pci_dev_id*)) {
	ASSERT(name && table && probe);

	node_t *node;
	foreach(node, drivers) {
		struct pci_driver *driver = (struct pci_driver *)node->data;
		if (strcmp(driver->name, name) == 0)
			break;
	}

	if (node)
		return -EEXIST;

	struct pci_driver *driver =
		(struct pci_driver *)kmalloc(sizeof(struct pci_driver));
	if (!driver)
		return -ENOMEM;

	driver->name = name;
	driver->table = table;
	driver->probe = probe;

	spin_lock(&pci_lock);
	node = list_insert(drivers, driver);
	if (!node) {
		spin_unlock(&pci_lock);
		kfree(driver);
		return -ENOMEM;
	}

	driver_search(driver);
	spin_unlock(&pci_lock);

	printf("PCI driver %s added\n", name);

	return 0;
}

int32_t claim_pci_dev(struct pci_dev *dev) {
	ASSERT(dev);
	int32_t ret = 0;
	if (dev->claimed) 
		ret = -1;
	else
		dev->claimed = 1;
	return ret;
}

void release_pci_dev(struct pci_dev *dev) {
	spin_lock(&pci_lock);
	dev->claimed = 0;
	spin_unlock(&pci_lock);
}