/* pci.h - PCI ops header */

/* Copyright (C) 2015 Bth8 <bth8fwd@gmail.com>
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

#ifndef PCI_H
#define PCI_H
#include <common.h>

#define CONFIG_ADDRESS	0x0CF8
#define CONFIG_DATA		0x0CFC

#define PCI_ANY			0xFFFF

#define PCI_DEVICE(v, d) {.vendor = v, .device = d, .class = PCI_ANY, .mask = PCI_ANY}
#define PCI_DEVICE_CLASS(c, m) {.vendor = PCI_ANY, .device = PCI_ANY, .class = c, .mask = m}

struct pci_dev;

struct pci_bus {
	struct pci_bus *parent;		// Parent bus
	struct pci_bus *children;	// Chain of busses under this one
	struct pci_bus *sibling;	// Next bus on parent bus
	struct pci_bus *next;		// Chain of all busses

	struct pci_dev *self;		// This bus as seen by its parent
	struct pci_dev *devs;		// Chain of devices on this bus

	uint8_t primary;			// Number of the parent bus
	uint8_t secondary;			// Number of this bus
	uint8_t subordinate;		// Config stuff
};

struct pci_dev {
	struct pci_bus *bus;		// Bus we're on
	struct pci_dev *sibling;	// Next device on this bus
	struct pci_dev *next;		// Chain of all devices

	uint8_t slot;				// Slot number
	uint8_t func;				// Function number (for multifunction devices)
	uint16_t vendor;			// Vendor number
	uint16_t device;			// Device ID
	uint16_t class;				// Class, subclass

	uint32_t claimed;
};

struct pci_dev_id {
	uint16_t vendor;
	uint16_t device;
	uint16_t class;
	uint16_t mask;
};

struct pci_driver {
	const char *name;
	const struct pci_dev_id *table;
	int (*probe)(struct pci_dev*, const struct pci_dev_id*);
};

inline uint32_t pciConfigReadDword(uint8_t bus, uint8_t slot, uint8_t func,
	uint8_t off);
inline uint16_t pciConfigReadWord(uint8_t bus, uint8_t slot, uint8_t func,
	uint8_t off);
inline uint8_t pciConfigReadByte(uint8_t bus, uint8_t slot, uint8_t func,
	uint8_t off);
inline void pciConfigWriteDword(uint8_t bus, uint8_t slot, uint8_t func,
	uint8_t off, uint32_t val);
inline void pciConfigWriteWord(uint8_t bus, uint8_t slot, uint8_t func,
	uint8_t off, uint16_t val);
inline void pciConfigWriteByte(uint8_t bus, uint8_t slot, uint8_t func,
	uint8_t off, uint8_t val);

void init_pci(void);
void dump_pci(void);
int32_t register_pci(const char *name, const struct pci_dev_id *table,
	int (*probe)(struct pci_dev*, const struct pci_dev_id*));
int32_t claim_pci_dev(struct pci_dev *dev);
void release_pci_dev(struct pci_dev *dev);

#endif /* PCI_H */
